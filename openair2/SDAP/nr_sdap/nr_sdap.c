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

#define _GNU_SOURCE
#include "nr_sdap.h"
#include "softmodem-common.h"
#include "openair2/LAYER2/RLC/rlc.h"
#include "common/utils/tun_if.h"
#include <pthread.h>

struct thread_args {
  ue_id_t ue_id;
  int pdu_session_id;
  int sock_fd;
  int qfi;
  int rqi;
  int gnb_flag;
};

static void *tun_read_thread(void *arg)
{
  struct thread_args *targs = (struct thread_args *)arg;
  int sock_fd = targs->sock_fd;
  ue_id_t UEid = targs->ue_id;
  int pdu_session_id = targs->pdu_session_id;
  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  int rb_id = 1;
  bool run_thread = true;

  while (run_thread) {
    len = read(sock_fd, &rx_buf, NL_MAX_PAYLOAD);

    if (len == -1) {
      LOG_E(SDAP, "error: cannot read() from fd %d: errno %d, %s\n", sock_fd, errno, strerror(errno));
      return NULL; /* exit thread */
    }

    LOG_D(SDAP, "read from tun returns len %d for pdusession id %d\n", len, pdu_session_id);

    protocol_ctxt_t ctxt = {.enb_flag = targs->gnb_flag, .rntiMaybeUEid = UEid};

    run_thread = sdap_data_req(&ctxt,
                               UEid,
                               SRB_FLAG_NO,
                               rb_id,
                               RLC_MUI_UNDEFINED,
                               RLC_SDU_CONFIRM_NO,
                               len,
                               (unsigned char *)rx_buf,
                               PDCP_TRANSMISSION_MODE_DATA,
                               NULL,
                               NULL,
                               targs->qfi,
                               targs->rqi,
                               pdu_session_id);
  }
  free(arg);

  return NULL;
}

void start_sdap_tun_gnb(int id)
{
  pthread_t t;

  struct thread_args *arg = malloc(sizeof(struct thread_args));
  char *ifprefix = get_softmodem_params()->nsa ? "oaitun_gnb" : "oaitun_enb";
  char ifname[20];
  snprintf(ifname, sizeof(ifname), "%s%d", ifprefix, id);
  tun_config(ifname, "10.0.1.1", NULL);
  arg->sock_fd = tun_alloc(ifname);
  {
    // default ue id & pdu session id in nos1 mode
    nr_sdap_entity_t *entity = nr_sdap_get_entity(1, get_softmodem_params()->default_pdu_session_id, SDAP_NO_LOCK);
    DevAssert(entity != NULL);
    entity->pdusession_sock = arg->sock_fd;
    arg->pdu_session_id = entity->pdusession_id;
    arg->ue_id = entity->ue_id;
    arg->rqi = false;
    arg->gnb_flag = 1;
    arg->qfi = 7;
  }
  threadCreate(&t, tun_read_thread, (void *)arg, "gnb_tun_read_thread", -1, OAI_PRIORITY_RT_LOW);
}

void start_sdap_tun_ue(ue_id_t ue_id, int pdu_session_id, int sock)
{
  struct thread_args *arg = malloc(sizeof(struct thread_args));
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id, SDAP_NO_LOCK);
  DevAssert(entity != NULL);
  arg->sock_fd = entity->pdusession_sock = sock;
  arg->ue_id = entity->ue_id;
  arg->pdu_session_id = entity->pdusession_id;
  arg->qfi = entity->qfi;
  arg->rqi = SDAP_HDR_UL_DATA_PDU;
  arg->gnb_flag = 0;
  entity->stop_thread = false;
  char thread_name[64];
  snprintf(thread_name, sizeof(thread_name), "ue_tun_read_%ld_p%d", ue_id, pdu_session_id);
  threadCreate(&entity->pdusession_thread, tun_read_thread, (void *)arg, thread_name, -1, OAI_PRIORITY_RT_LOW);
}

bool sdap_data_req(protocol_ctxt_t *ctxt_p,
                   const ue_id_t ue_id,
                   const srb_flag_t srb_flag,
                   const rb_id_t rb_id,
                   const mui_t mui,
                   const confirm_t confirm,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t pt_mode,
                   const uint32_t *sourceL2Id,
                   const uint32_t *destinationL2Id,
                   const uint8_t qfi,
                   const bool rqi,
                   const int pdusession_id) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id, SDAP_MUTEX_TX);

  if(sdap_entity == NULL) {
    LOG_E(SDAP, "Entity not found with ue: 0x%" PRIx64 " and pdusession id: %d\n", ue_id, pdusession_id);
    return false;
  }

  bool ret = sdap_entity->tx_entity(sdap_entity,
                                    ctxt_p,
                                    srb_flag,
                                    rb_id,
                                    mui,
                                    confirm,
                                    sdu_buffer_size,
                                    sdu_buffer,
                                    pt_mode,
                                    sourceL2Id,
                                    destinationL2Id,
                                    qfi,
                                    rqi);
  ret &= (!sdap_entity->stop_thread); // return false if data_req failed or stop thread is set
  nr_sdap_unlock(sdap_entity, SDAP_MUTEX_TX);
  return ret;
}

void sdap_data_ind(rb_id_t pdcp_entity,
                   int is_gnb,
                   bool has_sdap_rx,
                   int pdusession_id,
                   ue_id_t ue_id,
                   char *buf,
                   int size) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id, SDAP_MUTEX_RX);

  if (sdap_entity == NULL) {
    LOG_E(SDAP, "Entity not found for ue rnti/ue_id: %lx and pdusession id: %d\n", ue_id, pdusession_id);
    return;
  }

  sdap_entity->rx_entity(sdap_entity,
                         pdcp_entity,
                         is_gnb,
                         has_sdap_rx,
                         pdusession_id,
                         ue_id,
                         buf,
                         size);
  nr_sdap_unlock(sdap_entity, SDAP_MUTEX_RX);
}
