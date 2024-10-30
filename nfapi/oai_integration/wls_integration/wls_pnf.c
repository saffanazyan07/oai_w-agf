/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "wls_pnf.h"
#include "pnf.h"
#include "nr_nfapi_p7.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p7.h"

nfapi_pnf_p7_config_t *wls_p7_config = NULL;
void wls_pnf_set_p7_config(nfapi_pnf_p7_config_t *p7_config){
  wls_p7_config = p7_config;
}

void *wls_mac_alloc_buffer(uint32_t size, uint32_t loc);

static pthread_t *pwls_testmac_thread = NULL;
static WLS_MAC_CTX wls_mac_iface;
static pid_t gwls_pid = 0;
static uint32_t gToFreeListCnt[TO_FREE_SIZE] = {0};
static uint64_t gpToFreeList[TO_FREE_SIZE][TOTAL_FREE_BLOCKS] = {{0L}};
static uint8_t alloc_track[ALLOC_TRACK_SIZE];
static uint64_t gTotalTick = 0, gUsedTick = 0;


typedef void *WLS_HANDLE;
void *g_shmem;
uint64_t g_shmem_size;

WLS_HANDLE g_phy_wls;


uint8_t phy_dpdk_init(void);
uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize);
void phy_mac_recv();
void wls_mac_print_stats(void);

pnf_t *_this = NULL;

static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
    return &wls_mac_iface;
}


static nfapi_pnf_config_t *cfg ;

void *wls_fapi_pnf_nr_start_thread(void *ptr)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] IN WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  cfg = (nfapi_pnf_config_t *)ptr;
  struct sched_param sp;
  sp.sched_priority = 20;
  //pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
  wls_fapi_nr_pnf_start();
  return (void *)0;
}

int wls_fapi_nr_pnf_start()
{
  int64_t ret;
  // Verify that config is not null
  if (cfg == 0)
    return -1;

  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);

  _this = (pnf_t *)(cfg);
  _this->terminate = 0;
  // Init PNF config
  nfapi_pnf_phy_config_t* phy = (nfapi_pnf_phy_config_t*)malloc(sizeof(nfapi_pnf_phy_config_t));
  memset(phy, 0, sizeof(nfapi_pnf_phy_config_t));

  phy->state = NFAPI_PNF_PHY_IDLE;
  phy->phy_id = 0;
  phy->next = (_this->_public).phys;
  (_this->_public).phys = phy;
  _this->_public.state = NFAPI_PNF_RUNNING; // not a nFAPI PNF, go immediately to PNF_RUNNING state

  // DPDK init
  ret = phy_dpdk_init();
  if (!ret) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  DPDK Init - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] DPDK Init - Done\n");

  // WLS init
  ret = phy_wls_init(WLS_DEV_NAME, WLS_MAC_MEMORY_SIZE, WLS_PHY_MEMORY_SIZE);
  if (!ret) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  WLS Init - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] WLS Init - Done\n");

  // Start Receiving loop from the VNF
  printf("Start Receiving loop from the VNF\n");
  phy_mac_recv();
  // Should never get here, to be removed, align with present implementation of PNF loop
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Exiting...\n");

  return true;
}

uint8_t phy_dpdk_init(void)
{
  unsigned long i;
  char *argv[] = {"OAI_PNF", "--proc-type=primary", "--file-prefix", "wls", "--iova-mode=pa", "--no-pci"};
  int argc = RTE_DIM(argv);
  /* initialize EAL first */
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Calling rte_eal_init: ");

  for (i = 0; i < RTE_DIM(argv); i++) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s ", argv[i]);
  }

  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n");

  if (rte_eal_init(argc, argv) < 0)
    rte_panic("Cannot init EAL\n");

  return true;
}

uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize)
//uint8_t phy_wls_init(const char *dev_name,  uint64_t nBlockSize)
{
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  pWls->nTotalAllocCnt = 0;
  pWls->nTotalFreeCnt = 0;
  pWls->nTotalUlBufAllocCnt = 0;
  pWls->nTotalUlBufFreeCnt = 0;
  pWls->nTotalDlBufAllocCnt = 0;
  pWls->nTotalDlBufFreeCnt = 0;

  pWls->hWls = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
  if (pWls->hWls) {
    /* allocate chuck of memory */
    if (WLS_Alloc(pWls->hWls, nWlsMacMemSize + nWlsPhyMemSize) != NULL) {
      printf("WLS Memory allocated successfully\n");
    } else {
      printf("Unable to alloc WLS Memory\n");
      return false;
    }
  } else {
    printf("can't open WLS instance");
    return false;
  }
  return true;
}

#define FILL_FAPI_LIST_ELEM(_currElem, _nextElem, _msgType, _numMsgInBlock, _alignOffset)\
{\
   _currElem->msg_type             = (uint8_t) _msgType;\
   _currElem->num_message_in_block = _numMsgInBlock;\
   _currElem->align_offset         = (uint16_t) _alignOffset;\
   _currElem->msg_len              = _numMsgInBlock * _alignOffset;\
   _currElem->p_next               = _nextElem;\
   _currElem->p_tx_data_elm_list   = NULL;\
   _currElem->time_stamp           = 0;\
}

int wls_pnf_nr_pack_and_send_p5_message(pnf_t* pnf, nfapi_nr_p4_p5_message_header_t* msg, uint32_t msg_len)
{
  int packed_len = fapi_nr_p5_message_pack(msg, msg_len,
                                            pnf->tx_message_buffer,
                                            sizeof(pnf->tx_message_buffer),
                                            &pnf->_public.codec_config);

  if (packed_len < 0)
  {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p5_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  printf("fapi_nr_p5_message_pack succeeded having packed %d bytes\n", packed_len);
  printf("in msg, msg_len is %d\n", msg->message_length);
  // send the header first
  //g_phy_wls
  /*
   * typedef struct {
uint8_t num_msg;
// Can be used for Phy Id or Carrier Id  5G FAPI Table 3-2
uint8_t handle;
#ifndef OAI_TESTING
uint8_t pad[2];
#endif
} fapi_msg_header_t,
   * */
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();

  int num_avail_blocks =  WLS_NumBlocks(pWls->hWls);
  printf("num_avail_blocks is %d\n", num_avail_blocks);
  while (num_avail_blocks < 2) {
    num_avail_blocks =  WLS_NumBlocks(pWls->hWls);
    printf("num_avail_blocks is %d\n", num_avail_blocks);
  }
 /* get PA blocks for header and msg */
  uint64_t pa_hdr = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_hdr, "WLS_DequeueBlock failed for pa_hdr\n");
  uint64_t pa_msg = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_msg, "WLS_DequeueBlock failed for pa_msg\n");

  p_fapi_api_queue_elem_t  headerElem = WLS_PA2VA(pWls->hWls, pa_hdr);//WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t) + 2));//  (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t) + 2), 0);
  AssertFatal(headerElem, "WLS_PA2VA failed for headerElem\n");
  p_fapi_api_queue_elem_t  msgElem = WLS_PA2VA(pWls->hWls, pa_msg);//WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t) + 2));//   (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t)+packed_len-NFAPI_HEADER_LENGTH+6), 0);
  AssertFatal(headerElem, "WLS_PA2VA failed for msgElem\n");
  FILL_FAPI_LIST_ELEM(msgElem, NULL, msg->message_id, 1,  packed_len-NFAPI_HEADER_LENGTH+6);
  //copy just the body ( and message_id(2 bytes) and message_length(4 bytes) ) to the buffer
  memcpy((uint8_t *)(msgElem+1),(pnf->tx_message_buffer),packed_len);
  printf("Message body to send\n");
  for (int i = 0; i < packed_len - 2; ++i) {
    printf("0x%02x ",((uint8_t *)(msgElem+1))[i]);
  }
printf("\n");
  printf("PBORLA Message body to send\n");
  for (int i = 0; i < packed_len; ++i) {
    printf("0x%02x ",pnf->tx_message_buffer[i]);
  }
printf("\n");
  //fapi_message_header_t msgHeader = {.num_msg = 1, .opaque_handle = 0 ,.message_length = msg->message_length, .message_id = msg->message_id};
  uint8_t wls_header[] = {1,0}; // num_messages ,  opaque_handle
  p_fapi_api_queue_elem_t elem;
  FILL_FAPI_LIST_ELEM(headerElem, msgElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  //headerElem->message_body = malloc(sizeof(wls_header));
  memcpy((uint8_t *)(headerElem+1),wls_header, 2);
  printf("Message header to send \n");
  for (int i = 0; i < 2; ++i) {
    printf("0x%02x ",((uint8_t *)(headerElem+1))[i]);
  }
  printf("\n");
  int retval = 0;


  retval = wls_msg_send(pWls->hWls,headerElem);
  return retval;
}

static void wls_pnf_nr_handle_config_request(uint32_t msgSize, void *msg)
{
  if (msg == NULL || _this == NULL)
  {
    AssertFatal(msg != NULL && _this != NULL,"%s: NULL parameters\n", __FUNCTION__);
  }
  nfapi_nr_config_request_scf_t req = {0};
  nfapi_pnf_config_t* config = &(_this->_public);

  int unpack_result = fapi_nr_p5_message_unpack(msg, msgSize, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);

  req.carrier_config.dl_grid_size[1].value = 273;
  req.carrier_config.ul_grid_size[1].value = 273;

  // TODO: Process and use the message
  if(config->state == NFAPI_PNF_RUNNING)
  {
    nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
    if(phy)
    {
      if(phy->state != NFAPI_PNF_PHY_RUNNING)
      {
        if(config->nr_config_req)
        {
          (config->nr_config_req)(config, phy, &req);
        }
      }
      else
      {
        nfapi_nr_config_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_CONFIG_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_MSG_INVALID_STATE;
        printf("Try sending response back NFAPI_MSG_INVALID_STATE\n");
        nfapi_nr_pnf_config_resp(config, &resp);
      }
    }
    else
    {
      nfapi_nr_config_response_scf_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_CONFIG_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_MSG_INVALID_CONFIG;
      printf("Try sending response back NFAPI_MSG_INVALID_CONFIG\n");
      nfapi_nr_pnf_config_resp(config, &resp);
    }
  }
  else
  {
    nfapi_nr_config_response_scf_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_CONFIG_RESPONSE;
    resp.header.phy_id = req.header.phy_id;
    resp.error_code = NFAPI_MSG_INVALID_STATE;
    printf("Try sending response back NFAPI_MSG_INVALID_STATE\n");
    nfapi_nr_pnf_config_resp(config, &resp);
  }
  // Free at the end
      free_config_request(&req);
}


static void wls_pnf_nr_handle_start_request(uint32_t msgSize, void *msg)
{
  if (msg == NULL || _this == NULL) {
    AssertFatal(msg != NULL && _this != NULL, "%s: NULL parameters\n", __FUNCTION__);
  }

  nfapi_nr_start_request_scf_t req = {0};
  nfapi_pnf_config_t* config = &(_this->_public);

  int unpack_result = fapi_nr_p5_message_unpack(msg, msgSize, &req, sizeof(req), NULL);
  DevAssert(unpack_result >= 0);

  if(config->state == NFAPI_PNF_RUNNING)
  {
    nfapi_pnf_phy_config_t* phy = nfapi_pnf_phy_config_find(config, req.header.phy_id);
    if(phy)
    {
      if(phy->state != NFAPI_PNF_PHY_RUNNING)
      {
        if(config->nr_start_req)
        {
          (config->nr_start_req)(config, phy, &req);
        }
      }
      else
      {
        nfapi_nr_start_response_scf_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
        resp.header.phy_id = req.header.phy_id;
        resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
        nfapi_nr_pnf_start_resp(config, &resp);
      }
    }
    else
    {
      nfapi_nr_start_response_scf_t resp;
      memset(&resp, 0, sizeof(resp));
      resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
      resp.header.phy_id = req.header.phy_id;
      resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
      nfapi_nr_pnf_start_resp(config, &resp);
    }
  }
  else
  {
    nfapi_nr_start_response_scf_t resp;
    memset(&resp, 0, sizeof(resp));
    resp.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE;
    resp.header.phy_id = req.header.phy_id;
    resp.error_code = NFAPI_NR_START_MSG_INVALID_STATE;
    nfapi_nr_pnf_start_resp(config, &resp);
  }

}


int wls_pnf_nr_pack_and_send_p7_message(nfapi_nr_p7_message_header_t * msg)
{
  pnf_t* pnf = _this;
  int packed_len = fapi_nr_p7_message_pack(msg, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), NULL);

  if (packed_len < 0)
  {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p7_message_pack failed (%d)\n", packed_len);
    return -1;
  }
  PWLS_MAC_CTX pWls = wls_mac_get_ctx();
  /* get PA blocks for header and msg */
  uint64_t pa_hdr = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_hdr, "WLS_DequeueBlock failed for pa_hdr\n");
  uint64_t pa_msg = WLS_DequeueBlock(pWls->hWls);
  AssertFatal(pa_msg, "WLS_DequeueBlock failed for pa_msg\n");
  p_fapi_api_queue_elem_t  headerElem = WLS_PA2VA(pWls->hWls, pa_hdr);// WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t) + 2));// (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t) + 2), 0);
  AssertFatal(headerElem, "WLS_PA2VA failed for headerElem\n");
  p_fapi_api_queue_elem_t fapiMsgElem = WLS_PA2VA(pWls->hWls, pa_msg);//WLS_Alloc(pWls->hWls, (sizeof(fapi_api_queue_elem_t)+packed_len+NFAPI_HEADER_LENGTH));//  (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t)+packed_len+NFAPI_HEADER_LENGTH), 0);
  AssertFatal(fapiMsgElem, "WLS_PA2VA failed for fapiMsgElem\n");
  FILL_FAPI_LIST_ELEM(fapiMsgElem, NULL, msg->message_id, 1,  packed_len+NFAPI_HEADER_LENGTH);
  memcpy((uint8_t *)(fapiMsgElem +1),(pnf->tx_message_buffer),packed_len+NFAPI_HEADER_LENGTH);
  //fapi_message_header_t msgHeader = {.num_msg = 1, .opaque_handle = 0 ,.message_length = msg->message_length, .message_id = msg->message_id};
  uint8_t wls_header[] = {1,0}; // num_messages ,  opaque_handle
  p_fapi_api_queue_elem_t elem;
  FILL_FAPI_LIST_ELEM(headerElem, fapiMsgElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  //headerElem->message_body = malloc(sizeof(wls_header));
  memcpy((uint8_t *)(headerElem+1),wls_header, 2);
  int retval = 0;

  // /* Sending first block */
  // /* Call WLS_VA2PA to convert struct pointer to physical address */
  // if (WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls,headerElem), headerElem->msg_len +sizeof(p_fapi_api_queue_elem_t), headerElem->msg_type, WLS_SG_FIRST) != 0) {
  //   printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
  //   return false;
  // }
  // /* Sending second block */
  // if (WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls,fapiMsgElem), fapiMsgElem->msg_len + sizeof(p_fapi_api_queue_elem_t), fapiMsgElem->msg_type, WLS_SG_LAST) != 0) {
  //   printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
  //   return false;
  // }
  // return true;
  retval = wls_msg_send(pWls->hWls,headerElem);
  return retval;
}

static void wls_pnf_nr_handle_p7_messages(uint32_t msgSize, void *rcv_msg, int msgId){
  if (rcv_msg == NULL || _this == NULL)
  {
    AssertFatal(rcv_msg != NULL && _this != NULL,"%s: NULL parameters\n", __FUNCTION__);
  }
  uint8_t *msg = rcv_msg;
  uint8_t *hdr = malloc(NFAPI_HEADER_LENGTH);
  memcpy(&hdr, &rcv_msg, NFAPI_HEADER_LENGTH);
  nfapi_pnf_config_t* config = &(_this->_public);
  uint8_t *end = (uint8_t *)rcv_msg + msgSize + NFAPI_HEADER_LENGTH;
  // first, unpack the header
  fapi_msg_header fapi_msg;
  if (!pull8(&hdr, &fapi_msg.num_msg, end)) {
    AssertFatal(1 == 0, "FAPI num_msg unpack failed\n");
  }
  if (!pull8(&hdr, &fapi_msg.opaque_handle, end)) {
    AssertFatal(1 == 0, "FAPI opaque_handle unpack failed\n");
  }
  if (!pull16(&hdr, &fapi_msg.message_id, end)) {
    AssertFatal(1 == 0, "FAPI message_id unpack failed\n");
  }
  if (!pull32(&hdr, &fapi_msg.message_length, end)) {
    AssertFatal(1 == 0, "FAPI message_length unpack failed\n");
  }

  if (!check_nr_fapi_unpack_length(fapi_msg.message_id, fapi_msg.message_length + NFAPI_HEADER_LENGTH)) {
    printf("Message 0x%02x too short, ignoring\n", fapi_msg.message_id);
  } else {
    switch (msgId) {
      case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST: {
        nfapi_nr_dl_tti_request_t unpacked_msg = {.header.message_id = fapi_msg.message_id,
                                                  .header.message_length = fapi_msg.message_length};
        if (fapi_nr_p7_message_unpack(msg, msgSize + NFAPI_HEADER_LENGTH, &unpacked_msg, fapi_msg.message_length+NFAPI_HEADER_LENGTH, 0)!=0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_p7_config->dl_tti_req_fn != NULL);
        (wls_p7_config->dl_tti_req_fn)(NULL, (wls_p7_config), &unpacked_msg);
        free_dl_tti_request(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST: {
        nfapi_nr_ul_tti_request_t unpacked_msg = {.header.message_id = fapi_msg.message_id,
                                                  .header.message_length = fapi_msg.message_length};
        if (fapi_nr_p7_message_unpack(msg, msgSize + NFAPI_HEADER_LENGTH, &unpacked_msg, fapi_msg.message_length+NFAPI_HEADER_LENGTH, 0)!=0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_p7_config->ul_tti_req_fn != NULL);
        (wls_p7_config->ul_tti_req_fn)(NULL, (wls_p7_config), &unpacked_msg);
        free_ul_tti_request(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST: {
        nfapi_nr_ul_dci_request_t unpacked_msg = {.header.message_id = fapi_msg.message_id,
                                                  .header.message_length = fapi_msg.message_length};
        if (fapi_nr_p7_message_unpack(msg, msgSize + NFAPI_HEADER_LENGTH, &unpacked_msg, fapi_msg.message_length+NFAPI_HEADER_LENGTH, 0)!=0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_p7_config->ul_dci_req_fn != NULL);
        (wls_p7_config->ul_dci_req_fn)(NULL, (wls_p7_config), &unpacked_msg);
        free_ul_dci_request(&unpacked_msg);
      }
      break;
      case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST: {
        nfapi_nr_tx_data_request_t unpacked_msg = {.header.message_id = fapi_msg.message_id,
                                                   .header.message_length = fapi_msg.message_length};
        if (fapi_nr_p7_message_unpack(msg, msgSize + NFAPI_HEADER_LENGTH, &unpacked_msg, fapi_msg.message_length+NFAPI_HEADER_LENGTH, 0)!=0) {
          printf("Failure unpacking, or dummy message, ignoring\n");
          break;
        }
        DevAssert(wls_p7_config->tx_data_req_fn != NULL);
        (wls_p7_config->tx_data_req_fn)((wls_p7_config), &unpacked_msg);
        free_tx_data_request(&unpacked_msg);
      }
      break;
      default:
        break;
    }
  }
}

static void procPhyMessages(uint32_t msgSize, void *msg, uint16_t msgId)
{
  // Should be able to be casted into already present fapi p5 header format, to fix, dependent on changes on L2 to follow
  // fapi_message_header_t in nr_fapi.h
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "[PHY] Received Msg ID 0x%02x\n", msgId);

  switch (msgId) {
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST: {
#if 0 // To print TLVs, only for debug purposes
      fapi_config_req_t *wls_conf_req = (fapi_config_req_t *)msg;
      printf("number_of_tlvs %d \n", wls_conf_req->number_of_tlvs);
      for (int i = 0; i < wls_conf_req->number_of_tlvs; ++i) {
        printf("TLV #%d tag 0x%02x length 0x%02x value 0x%02x\n",
               i,
               wls_conf_req->tlvs[i].tl.tag,
               wls_conf_req->tlvs[i].tl.length,
               wls_conf_req->tlvs[i].value);
      }
#endif
      wls_pnf_nr_handle_config_request(msgSize, msg);

      break;
    }
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST: {
      printf("\n NFAPI_NR_PHY_MSG_TYPE_START_REQUEST");
      wls_pnf_nr_handle_start_request(msgSize, msg);
      break;
    }

    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST ... NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION : {
        wls_pnf_nr_handle_p7_messages(msgSize, msg, msgId);
        break;
      }
    default:
      break;
  }
}

void phy_mac_recv()
{
  /* Number of Memory blocks to get */
  uint32_t msgSize;
  uint16_t msgType;
  uint16_t flags;
  uint32_t i = 0;
  PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
  while (_this->terminate == 0) {
    int numMsgToGet = WLS_Wait(pWls->hWls);
    if (numMsgToGet <= 0) {
      continue;
    }
    while (numMsgToGet--) {
      p_fapi_api_queue_elem_t msg  = NULL;
      const uint64_t msg_PA = WLS_Get(pWls->hWls , &msgSize, &msgType, &flags);
      if (msg_PA != 0) {
        i++;
        void* msg_VA = WLS_PA2VA(pWls->hWls , msg_PA);
        msg = (p_fapi_api_queue_elem_t)msg_VA;
        printf("\n");

        if (msg->msg_type != FAPI_VENDOR_MSG_HEADER_IND) {
          procPhyMessages(msg->msg_len, (void *)(msg + 1), msg->msg_type);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "\n[PHY] MAC2PHY WLS Get Error for msg %d\n", i);
        break;
      }
      if (flags & WLS_TF_FIN) {
        // Don't return, the messages responses will be handled in procPhyMessages
      }
    
    }
  }
}
