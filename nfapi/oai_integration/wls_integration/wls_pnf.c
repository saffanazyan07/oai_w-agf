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
/*! \file nfapi/oai_integration/wls_integration/wls_pnf.c
 * \brief
 * \author Ruben S. Silva
 * \date 2024
 * \version 0.1
 * \company OpenAirInterface Software Alliance
 * \email: contact@openairinterface.org, rsilva@allbesmart.pt
 * \note
 * \warning
 */

#include "nfapi/oai_integration/wls_integration/include/wls_pnf.h"
#include "pnf.h"
#include "nr_nfapi_p7.h"
#define WLS_TEST_DEV_NAME "wls"
#define WLS_TEST_MSG_ID 1
#define WLS_TEST_MSG_SIZE 100
#define WLS_MAC_MEMORY_SIZE 0x3EA80000
#define WLS_PHY_MEMORY_SIZE 0x18000000
#define NUM_PHY_MSGS 16

#define MAX_NUM_LOCATIONS           (50)

#define MIN_DL_BUF_LOCATIONS        (0)                                             /* Used for stats collection 0-49 */
#define MIN_UL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)      /* Used for stats collection 50-99 */

#define MAX_DL_BUF_LOCATIONS        (MIN_DL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 0-49 */
#define MAX_UL_BUF_LOCATIONS        (MIN_UL_BUF_LOCATIONS + MAX_NUM_LOCATIONS)          /* Used for stats collection 50-99 */

#define WLS_MSG_BLOCK_SIZE          ( 16384 * 16 )

#define MSG_MAXSIZE                         ( 16384 * 16 )


#define TO_FREE_SIZE                        ( 10 )
#define TOTAL_FREE_BLOCKS                   ( 50 * 12)
#define ALLOC_TRACK_SIZE                    ( 16384 )

//#define MEMORY_CORRUPTION_DETECT
#define MEMORY_CORRUPTION_DETECT_FLAG       (0xAB)


void *wls_mac_alloc_buffer(uint32_t size, uint32_t loc);
typedef struct wls_mac_mem_array
{
    void **ppFreeBlock;
    void *pStorage;
    void *pEndOfStorage;
    uint32_t nBlockSize;
    uint32_t nBlockCount;
} WLS_MAC_MEM_SRUCT, *PWLS_MAC_MEM_SRUCT;

typedef struct wls_mac_ctx
{
    void *hWls;
    void *pWlsMemBase;
    void *pWlsMemBaseUsable;
    WLS_MAC_MEM_SRUCT sWlsStruct;

    uint64_t nTotalMemorySize;
    uint64_t nTotalMemorySizeUsable;
    uint32_t nBlockSize;
    uint32_t nTotalBlocks;
    uint32_t nAllocBlocks;
    uint32_t nTotalAllocCnt;
    uint32_t nTotalFreeCnt;
    uint32_t nTotalUlBufAllocCnt;
    uint32_t nTotalUlBufFreeCnt;
    uint32_t nTotalDlBufAllocCnt;
    uint32_t nTotalDlBufFreeCnt;
//  Support for FAPI Translator
    uint32_t nPartitionMemSize;
    void     *pPartitionMemBase;

    volatile pthread_mutex_t lock;
    volatile pthread_mutex_t lock_alloc;
} WLS_MAC_CTX, *PWLS_MAC_CTX;

static pthread_t *pwls_testmac_thread = NULL;
static WLS_MAC_CTX wls_mac_iface;
static int gwls_mac_ready = 0;
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
uint64_t phy_mac_recv();
uint8_t phy_mac_send(void *);
void wls_mac_print_stats(void);

pnf_t *_this = NULL;

static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
    return &wls_mac_iface;
}

uint64_t wls_mac_va_to_pa(void *ptr)
{
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    uint64_t ret = (uint64_t)WLS_VA2PA(pWls->hWls, ptr);

    //printf("wls_mac_va_to_pa: %p ->%p\n", ptr, (void*)ret);

    return ret;
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

uint32_t wls_mac_free_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void *pBlock)
{
    int idx;
    unsigned long mask = (((unsigned long)pMemArray->nBlockSize) - 1);

    pBlock = (void *)((unsigned long)pBlock & ~mask);

    if ((pBlock < pMemArray->pStorage) || (pBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_free_mem_array WARNING: Trying to free foreign block;Arr=%p,Blk=%p pStorage [%p .. %p]\n",
               pMemArray, pBlock, pMemArray->pStorage, pMemArray->pEndOfStorage);

        return false;
    }

    idx = (int)(((uint64_t)pBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;

    if (alloc_track[idx] == 0)
    {
        printf("wls_mac_free_mem_array ERROR: Double free Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            pBlock);

        return true;

    }
    else
    {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *)pBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++)
        {
            if (p[i] != MEMORY_CORRUPTION_DETECT_FLAG)
            {
                printf("ERROR: Corruption\n");
                wls_mac_print_stats();
                exit(-1);
            }
        }
#endif
        alloc_track[idx] = 0;
    }

    if (((void *) pMemArray->ppFreeBlock) == pBlock)
    {
        // Simple protection against freeing of already freed block
        return true;
    }
    // FIXME: Remove after debugging
    if ((pMemArray->ppFreeBlock != NULL)
        && (((void *) pMemArray->ppFreeBlock < pMemArray->pStorage)
        || ((void *) pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage)))
    {
        printf("wls_mac_free_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return false;
    }

    // FIXME: Remove after debugging
    if ((pBlock < pMemArray->pStorage) ||
        (pBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_free_mem_array ERROR: Invalid block;Arr=%p,Blk=%p\n",
                pMemArray, pBlock);
        return false;
    }

    *((void **)pBlock) = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    pMemArray->ppFreeBlock = (void **) ((unsigned long)pBlock & 0xFFFFFFFFFFFFFFF0);

    //printf("Block freed [%p,%p]\n", pMemArray, pBlock);

    return true;
}


void wls_mac_free_buffer(void *pMsg, uint32_t loc)
{
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    PWLS_MAC_MEM_SRUCT pMemArray = &pWls->sWlsStruct;

    //pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);

    //printf("----------------wls_mac_free_buffer: buf[%p] loc[%d]\n", pMsg, loc);
    if (wls_mac_free_mem_array(&pWls->sWlsStruct, (void *)pMsg) == true)
    {
        pWls->nAllocBlocks--;
    }
    else
    {
        printf("wls_mac_free_buffer Free error\n");
        wls_mac_print_stats();
        exit(-1);
    }

    pWls->nTotalFreeCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufFreeCnt++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufFreeCnt++;

    //pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);
}

int wls_mac_add_blocks_to_ul(void)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

    int res = 1;

    while(res == 1){
      void *pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+0);
      if(pMsg != NULL){
        res = WLS_EnqueueBlock(pWls,(uint64_t) WLS_VA2PA(pWls,pMsg));
        if(res == 0){

        }
      }
    }
    return res;


//    if(pMsg)
//    {
//        /* allocate blocks for UL transmittion */
//        while(WLS_EnqueueBlock(pWls->hWls,(uint64_t)wls_mac_va_to_pa(pMsg)))
//        {
//            ret++;
//            pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+1);
//            if(WLS_EnqueueBlock(pWls->hWls,(uint64_t)wls_mac_va_to_pa(pMsg)))
//            {
//                ret++;
//                pMsg = wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+1);
//            }
//
//            if(!pMsg)
//                break;
//        }
//
//        // free not enqueued block
//        if(pMsg)
//        {
//            wls_mac_free_buffer(pMsg, MIN_UL_BUF_LOCATIONS+3);
//        }
//    }
//
//    return ret;
}


int wls_fapi_nr_pnf_start()
{
  int64_t ret;
  uint64_t p_msg;
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
  ret = phy_wls_init(WLS_TEST_DEV_NAME, WLS_MAC_MEMORY_SIZE, WLS_PHY_MEMORY_SIZE);
  if (!ret) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  WLS Init - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] WLS Init - Done\n");

  // Start Receiving loop from the VNF
  printf("Start Receiving loop from the VNF\n");
  p_msg = phy_mac_recv();
  if (!p_msg) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "[PNF]  Receive from FAPI - Failed\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Receive from FAPI - Done\n");


  // Should never get here, to be removed, align with present implementation of PNF loop
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] Exiting...\n");

  return true;
}

uint8_t phy_dpdk_init(void)
{
  unsigned long i;
  char *argv[] = {"phy_app", "--proc-type=primary", "--file-prefix", "wls", "--iova-mode=pa", "--no-pci"};
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


uint32_t wls_mac_create_mem_array(PWLS_MAC_MEM_SRUCT pMemArray,
                              void *pMemArrayMemory,
                              uint32_t totalSize, uint32_t nBlockSize)
{
    int numBlocks = totalSize / nBlockSize;
    void **ptr;
    uint32_t i;

    printf("wls_mac_create_mem_array: pMemArray[%p] pMemArrayMemory[%p] totalSize[%d] nBlockSize[%d] numBlocks[%d]\n",
        pMemArray, pMemArrayMemory, totalSize, nBlockSize, numBlocks);

    // Can't be less than pointer size
    if (nBlockSize < sizeof(void *))
    {
        return false;
    }

    // Can't be less than one block
    if (totalSize < sizeof(void *))
    {
        return false;
    }

    pMemArray->ppFreeBlock = (void **)pMemArrayMemory;
    pMemArray->pStorage = pMemArrayMemory;
    pMemArray->pEndOfStorage = ((unsigned long*)pMemArrayMemory) + numBlocks * nBlockSize / sizeof(unsigned long);
    pMemArray->nBlockSize = nBlockSize;
    pMemArray->nBlockCount = numBlocks;

    // Initialize single-linked list of free blocks;
    ptr = (void **)pMemArrayMemory;
    for (i = 0; i < pMemArray->nBlockCount; i++)
    {
#ifdef MEMORY_CORRUPTION_DETECT
        // Fill with some pattern
        uint8_t *p = (uint8_t *)ptr;
        uint32_t j;

        p += (nBlockSize - 16);
        for (j = 0; j < 16; j++)
        {
            p[j] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif

        if (i == pMemArray->nBlockCount - 1)
        {
            *ptr = NULL;      // End of list
        }
        else
        {
            // Points to the next block
            *ptr = (void **)(((uint8_t*)ptr) + nBlockSize);
            ptr += nBlockSize / sizeof(unsigned long);
        }
    }

    memset(alloc_track, 0, sizeof(uint8_t) * ALLOC_TRACK_SIZE);

    return true;
}

int wls_mac_create_partition(PWLS_MAC_CTX pWls)
{
    memset(pWls->pWlsMemBase, 0xCC, pWls->nTotalMemorySize);
    pWls->pPartitionMemBase = pWls->pWlsMemBase;
    pWls->nPartitionMemSize = pWls->nTotalMemorySize/2;
    pWls->nTotalBlocks = pWls->nTotalMemorySize / MSG_MAXSIZE;
    return wls_mac_create_mem_array(&pWls->sWlsStruct, pWls->pPartitionMemBase, pWls->nPartitionMemSize, MSG_MAXSIZE);
}

uint32_t wls_mac_alloc_mem_array(PWLS_MAC_MEM_SRUCT pMemArray, void **ppBlock)
{
    int idx;

    if (pMemArray->ppFreeBlock == NULL)
    {
        printf("wls_mac_alloc_mem_array pMemArray->ppFreeBlock = NULL\n");
        return false;
    }

    // FIXME: Remove after debugging
    if (((void *) pMemArray->ppFreeBlock < pMemArray->pStorage) ||
        ((void *) pMemArray->ppFreeBlock >= pMemArray->pEndOfStorage))
    {
        printf("wls_mac_alloc_mem_array ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock);
        return false;
    }

    pMemArray->ppFreeBlock = (void **)((unsigned long)pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);
    *pMemArray->ppFreeBlock = (void **)((unsigned long)*pMemArray->ppFreeBlock & 0xFFFFFFFFFFFFFFF0);

    if ((*pMemArray->ppFreeBlock != NULL) &&
        (((*pMemArray->ppFreeBlock) < pMemArray->pStorage) ||
        ((*pMemArray->ppFreeBlock) >= pMemArray->pEndOfStorage)))
    {
        fprintf(stderr, "ERROR: Corrupted MemArray;Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
                pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
                *pMemArray->ppFreeBlock);
        return false;
    }

    *ppBlock = (void *) pMemArray->ppFreeBlock;
    pMemArray->ppFreeBlock = (void **) (*pMemArray->ppFreeBlock);

    idx = (((uint64_t)*ppBlock - (uint64_t)pMemArray->pStorage)) / pMemArray->nBlockSize;
    if (alloc_track[idx])
    {
        printf("wls_mac_alloc_mem_array Double alloc Arr=%p,Stor=%p,Free=%p,Curr=%p\n",
            pMemArray, pMemArray->pStorage, pMemArray->ppFreeBlock,
            *pMemArray->ppFreeBlock);
    }
    else
    {
#ifdef MEMORY_CORRUPTION_DETECT
        uint32_t nBlockSize = pMemArray->nBlockSize, i;
        uint8_t *p = (uint8_t *)*ppBlock;

        p += (nBlockSize - 16);
        for (i = 0; i < 16; i++)
        {
            p[i] = MEMORY_CORRUPTION_DETECT_FLAG;
        }
#endif
        alloc_track[idx] = 1;
    }

    //printf("Block allocd [%p,%p]\n", pMemArray, *ppBlock);

    return true;
}




void *wls_mac_alloc_buffer(uint32_t size, uint32_t loc)
{
    void *pBlock = NULL;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    PWLS_MAC_MEM_SRUCT pMemArray = &pWls->sWlsStruct;

    //pthread_mutex_lock((pthread_mutex_t *)&pWls->lock_alloc);

    if (wls_mac_alloc_mem_array(&pWls->sWlsStruct, &pBlock) != true)
    {
        printf("wls_mac_alloc_buffer alloc error size[%d] loc[%d]\n", size, loc);
        wls_mac_print_stats();
        exit(-1);
    }
    else
    {
        pWls->nAllocBlocks++;
    }

    //printf("----------------wls_mac_alloc_buffer: size[%d] loc[%d] buf[%p] nAllocBlocks[%d]\n", size, loc, pBlock, pWls->nAllocBlocks);

    //printf("[%p]\n", pBlock);

    pWls->nTotalAllocCnt++;
    if (loc < MAX_DL_BUF_LOCATIONS)
        pWls->nTotalDlBufAllocCnt++;
    else if (loc < MAX_UL_BUF_LOCATIONS)
        pWls->nTotalUlBufAllocCnt++;

    //pthread_mutex_unlock((pthread_mutex_t *)&pWls->lock_alloc);

    return pBlock;
}

void wls_mac_print_stats(void)
{
    PWLS_MAC_CTX pWls = wls_mac_get_ctx();

    printf("wls_mac_free_list_all:\n");
    printf("        nTotalBlocks[%d] nAllocBlocks[%d] nFreeBlocks[%d]\n", pWls->nTotalBlocks, pWls->nAllocBlocks, (pWls->nTotalBlocks- pWls->nAllocBlocks));
    printf("        nTotalAllocCnt[%d] nTotalFreeCnt[%d] Diff[%d]\n", pWls->nTotalAllocCnt, pWls->nTotalFreeCnt, (pWls->nTotalAllocCnt- pWls->nTotalFreeCnt));
    printf("        nDlBufAllocCnt[%d] nDlBufFreeCnt[%d] Diff[%d]\n", pWls->nTotalDlBufAllocCnt, pWls->nTotalDlBufFreeCnt, (pWls->nTotalDlBufAllocCnt- pWls->nTotalDlBufFreeCnt));
    printf("        nUlBufAllocCnt[%d] nUlBufFreeCnt[%d] Diff[%d]\n\n", pWls->nTotalUlBufAllocCnt, pWls->nTotalUlBufFreeCnt, (pWls->nTotalUlBufAllocCnt- pWls->nTotalUlBufFreeCnt));
}


uint8_t phy_wls_init(const char *dev_name, uint64_t nWlsMacMemSize, uint64_t nWlsPhyMemSize)
//uint8_t phy_wls_init(const char *dev_name,  uint64_t nBlockSize)
{
    uint32_t ret = false;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
    uint8_t *pMemZone;
    static const struct rte_memzone *mng_memzone;

    sleep(1);

    //pthread_mutex_init((pthread_mutex_t *)&pWls->lock, NULL);
    //pthread_mutex_init((pthread_mutex_t *)&pWls->lock_alloc, NULL);

    pWls->nTotalAllocCnt = 0;
    pWls->nTotalFreeCnt = 0;
    pWls->nTotalUlBufAllocCnt = 0;
    pWls->nTotalUlBufFreeCnt = 0;
    pWls->nTotalDlBufAllocCnt = 0;
    pWls->nTotalDlBufFreeCnt = 0;

//    	        pWls->hWls = WLS_Open(wls_device_name, WLS_MASTER_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
  //g_phy_wls = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
   pWls->hWls  = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
    if (pWls->hWls)
    {
        /* allocate chuck of memory */
	    pWls->pWlsMemBase= WLS_Alloc(pWls->hWls,  nWlsMacMemSize + nWlsPhyMemSize);
        //pWls->pWlsMemBase = WLS_Alloc(pWls->hWls, nWlsMacMemSize+nWlsPhyMemSize);
        if (pWls->pWlsMemBase)
        {
            pWls->nTotalMemorySize = nWlsMacMemSize;
            // pWls->nBlockSize       = wls_mac_check_block_size(nBlockSize);

            ret = wls_mac_create_partition(pWls);

            if (ret == true)
            {
                int nBlocks = 0;
                gwls_mac_ready = 1;

                nBlocks = WLS_EnqueueBlock(pWls->hWls, wls_mac_va_to_pa(wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+2)));
                /* allocate blocks for UL transmition */
                while(WLS_EnqueueBlock(pWls->hWls, wls_mac_va_to_pa(wls_mac_alloc_buffer(0, MIN_UL_BUF_LOCATIONS+3))))
                {
                    nBlocks++;
                }

                printf("WLS inited ok [%d]\n\n", nBlocks);
            }
            else
            {
                printf("can't create WLS Partition");
                return false;
            }

        }
        else
        {
            printf("can't allocate WLS memory");
            return false;
        }
    }
    else
    {
        printf("can't open WLS instance");
        return false;
    }

    return true;

#if 0
  g_phy_wls = WLS_Open(dev_name, WLS_SLAVE_CLIENT, &nWlsMacMemSize, &nWlsPhyMemSize);
  if (NULL == g_phy_wls) {
    return false;
  }
  g_shmem_size = nWlsMacMemSize + nWlsPhyMemSize;

  g_shmem = WLS_Alloc(g_phy_wls, g_shmem_size);
  if (NULL == g_shmem) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unable to alloc WLS Memory\n");
    return false;
  }
  return true;
#endif
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

int wls_pnf_nr_pack_and_send_p5_message(pnf_t* pnf, nfapi_p4_p5_message_header_t* msg, uint32_t msg_len)
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

  p_fapi_api_queue_elem_t  headerElem = (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t) + 2), 0);
  p_fapi_api_queue_elem_t  cfgReqQElem = (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t)+packed_len-NFAPI_HEADER_LENGTH+6), 0); 
  FILL_FAPI_LIST_ELEM(cfgReqQElem, NULL, msg->message_id, 1,  packed_len-NFAPI_HEADER_LENGTH+6);
  //copy just the body ( and message_id(2 bytes) and message_length(4 bytes) ) to the buffer
  memcpy((uint8_t *)(cfgReqQElem+1),(pnf->tx_message_buffer),packed_len);
  printf("Message body to send\n");
  for (int i = 0; i < packed_len - 2; ++i) {
    printf("0x%02x ",((uint8_t *)(cfgReqQElem+1))[i]);
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
  FILL_FAPI_LIST_ELEM(headerElem, cfgReqQElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  //headerElem->message_body = malloc(sizeof(wls_header));
  memcpy((uint8_t *)(headerElem+1),wls_header, 2);
  printf("Message header to send \n");
  for (int i = 0; i < 2; ++i) {
    printf("0x%02x ",((uint8_t *)(headerElem+1))[i]);
  }
  printf("\n");
  int retval = 0;


  retval = phy_mac_send(headerElem);
  // send the message body after
  //return pnf_send_message(pnf, pnf->tx_message_buffer, packed_len, 0/*msg->stream_id*/);

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

static int wls_fapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf,  uint32_t packedBufLen, nfapi_p7_codec_config_t *config)
{
  if (pMessageBuf == NULL || pPackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack supplied pointers are null\n");
    return -1;
  }

  nfapi_p7_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *end = pPackedBuf + packedBufLen;
  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *pPackMessageEnd = pPackedBuf + packedBufLen;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];
  uint8_t *pPacketBodyField = &pWritePackedMessage[8];
  uint8_t *pPacketBodyFieldStart = &pWritePackedMessage[8];

  // PHY API message header
  // Number of messages [0]
  // Opaque handle [1]
  // PHY API Message structure
  // Message type ID [2,3]
  // Message Length [4,5,6,7]
  // Message Body [8,...]
  if (!(push8(1, &pWritePackedMessage, pPackMessageEnd) && push8(0, &pWritePackedMessage, pPackMessageEnd)
        && push16(pMessageHeader->message_id, &pWritePackedMessage, pPackMessageEnd))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack header failed\n");
    return -1;
  }

  // look for the specific message
  uint8_t result = 0;
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      result = pack_dl_tti_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      result = pack_ul_tti_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      // TX_DATA.request already handled by aerial_pack_tx_data_request
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      result = pack_ul_dci_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_UE_RELEASE_REQUEST:
      result = pack_ue_release_request(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_UE_RELEASE_RESPONSE:
      result = pack_ue_release_response(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
      result = pack_nr_slot_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      result = pack_nr_rx_data_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      result = pack_nr_crc_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      result = pack_nr_uci_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      result = pack_nr_srs_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      result = pack_nr_rach_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      result = pack_nr_dl_node_sync(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
      result = pack_nr_ul_node_sync(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case NFAPI_TIMING_INFO:
      result = pack_nr_timing_info(pMessageHeader, &pPacketBodyField, end, config);
      break;

    case 0x8f:
      result = pack_nr_slot_indication(pMessageHeader, &pPacketBodyField, end, config);
      break;

    default: {
      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->pack_p7_vendor_extension) {
          result = (config->pack_p7_vendor_extension)(pMessageHeader, &pPacketBodyField, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "%s VE NFAPI message ID %d. No ve ecoder provided\n",
                      __FUNCTION__,
                      pMessageHeader->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }
    } break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack failed to pack message\n");
    return -1;
  }

  // check for a valid message length
  uintptr_t msgHead = (uintptr_t)pPacketBodyFieldStart;
  uintptr_t msgEnd = (uintptr_t)pPacketBodyField;
  uint32_t packedMsgLen = msgEnd - msgHead;
  uint16_t packedMsgLen16;
  if (packedMsgLen > 0xFFFF || packedMsgLen > packedBufLen) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Packed message length error %d, buffer supplied %d\n", packedMsgLen, packedBufLen);
    return -1;
  } else {
    packedMsgLen16 = (uint16_t)packedMsgLen;
  }

  // Update the message length in the header
  pMessageHeader->message_length = packedMsgLen16;

  // Update the message length in the header
  if (!push32(packedMsgLen, &pPackedLengthField, pPackMessageEnd))
    return -1;

  if (1) {
    // quick test
    if (pMessageHeader->message_length != packedMsgLen) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR,
                  "nfapi packedMsgLen(%d) != message_length(%d) id %d\n",
                  packedMsgLen,
                  pMessageHeader->message_length,
                  pMessageHeader->message_id);
    }
  }

  return (packedMsgLen16);
}


int wls_pnf_nr_pack_and_send_p7_message(nfapi_p7_message_header_t * msg)
{
  pnf_t* pnf = _this;
  int packed_len = wls_fapi_nr_p7_message_pack(msg, pnf->tx_message_buffer, sizeof(pnf->tx_message_buffer), NULL);

  if (packed_len < 0)
  {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "nfapi_p7_message_pack failed (%d)\n", packed_len);
    return -1;
  }

  printf("fapi_nr_p7_message_pack succeeded having packed %d bytes\n", packed_len);
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

  //p_fapi_api_queue_elem_t  headerElem = malloc((sizeof(fapi_api_queue_elem_t) + 2)); // 2 is for num_msg and opaque_handle2yy
  //p_fapi_api_queue_elem_t  cfgReqQElem = malloc(sizeof(fapi_api_queue_elem_t)+packed_len-NFAPI_HEADER_LENGTH+6); // body of the message
  p_fapi_api_queue_elem_t  headerElem = (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t) + 2), 0);
  p_fapi_api_queue_elem_t fapiMsgElem = (p_fapi_api_queue_elem_t)wls_mac_alloc_buffer((sizeof(fapi_api_queue_elem_t)+packed_len-NFAPI_HEADER_LENGTH+6), 0);
  FILL_FAPI_LIST_ELEM(fapiMsgElem, NULL, msg->message_id, 1,  packed_len-NFAPI_HEADER_LENGTH+6);
  //copy just the body ( and message_id(2 bytes) and message_length(4 bytes) ) to the buffer
  //memcpy((uint8_t *)(cfgReqQElem+1),(pnf->tx_message_buffer + 2),packed_len - 2);
  memcpy((uint8_t *)(fapiMsgElem +1),(pnf->tx_message_buffer),packed_len);
  printf("Message body to send\n");
  for (int i = 0; i < packed_len - 2; ++i) {
    printf("0x%02x ",((uint8_t *)(fapiMsgElem +1))[i]);
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
  FILL_FAPI_LIST_ELEM(headerElem, fapiMsgElem, FAPI_VENDOR_MSG_HEADER_IND, 1, 2);
  //headerElem->message_body = malloc(sizeof(wls_header));
  memcpy((uint8_t *)(headerElem+1),wls_header, 2);
  printf("Message header to send \n");
  for (int i = 0; i < 2; ++i) {
    printf("0x%02x ",((uint8_t *)(headerElem+1))[i]);
  }
  printf("\n");
  int retval = 0;


  retval = phy_mac_send(headerElem);
  // send the message body after
  //return pnf_send_message(pnf, pnf->tx_message_buffer, packed_len, 0/*msg->stream_id*/);

  //free(headerElem);
  //free(cfgReqQElem);
  return retval;
}

static void wls_pnf_nr_handle_p7_messages(uint32_t msgSize, void *msg, int msgId){
  if (msg == NULL || _this == NULL)
  {
    AssertFatal(msg != NULL && _this != NULL,"%s: NULL parameters\n", __FUNCTION__);
  }
  nfapi_pnf_config_t* config = &(_this->_public);
  uint8_t *end = (uint8_t *)msg + msgSize + NFAPI_HEADER_LENGTH;
  // first, unpack the header
  fapi_msg_header fapi_msg;
  if (!(pull8(msg, &fapi_msg.num_msg, end) && pull8(msg, &fapi_msg.opaque_handle, end)
        && pull16(msg, &fapi_msg.message_id, end)
        && pull32(msg, &fapi_msg.message_length, end))) {
    AssertFatal(1==0, "FAPI message header unpack failed\n");
  }

  switch (msgId) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      nfapi_nr_dl_tti_request_t dl_tti_req;
      dl_tti_req.header.message_id = fapi_msg.message_id;
      dl_tti_req.header.message_length = fapi_msg.message_length;

     /* unpack_dl_tti_request(msg,  end, &dl_tti_req, config);

      if (((vnf_info *)vnf_config->user_data)->p7_vnfs->config->nr_crc_indication) {
        (((vnf_info *)vnf_config->user_data)->p7_vnfs->config->nr_crc_indication)(&crc_ind);
        free(dl_tti_req.crc_list);
      }*/
      break;
    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      break;
    default:
      break;
  }
}

static void procPhyMessages(uint32_t msgSize, void *msg, uint16_t msgId)
{
  // Should be able to be casted into already present fapi p5 header format, to fix, dependent on changes on L2 to follow
  // fapi_message_header_t in nr_fapi.h
  printf("[PHY] Received Msg ID 0x%02x\n", msgId);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PHY] Received Msg ID 0x%02x\n", msgId);

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

uint64_t phy_mac_recv()
{
  int32_t numMsgToGet; /* Number of Memory blocks to get */
  uint64_t l2Msg; /* Message received */
  void *l2MsgPtr;
  uint32_t msgSize;
  uint16_t msgType;
  uint16_t flags;
  uint32_t i = 0;
  PWLS_MAC_CTX pWls =  wls_mac_get_ctx();
  p_fapi_api_queue_elem_t currElem = NULL;
  while (_this->terminate == 0) {
    printf("Before WLS_Wait\n");
    numMsgToGet = WLS_Wait(pWls->hWls);
    if (numMsgToGet <= 0) {
      continue;
    }
    printf("After WLS_Wait, numMsgToGet = %d\n",numMsgToGet);

    while (numMsgToGet--) {
      currElem = NULL;
      l2Msg = (uint64_t)NULL;
      l2MsgPtr = NULL;
      printf("Before WLS_Get\n");
      l2Msg = WLS_Get(pWls->hWls , &msgSize, &msgType, &flags);
      printf("After WLS_Get\n");
      if (l2Msg) {
        printf("\n[PHY] MAC2PHY WLS Received Block %d\n", i);
        NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[PHY] MAC2PHY WLS Received Block %d\n", i);
        i++;
        l2MsgPtr = WLS_PA2VA(pWls->hWls , l2Msg);
        currElem = (p_fapi_api_queue_elem_t)l2MsgPtr;

/*
        uint8_t * msgt = (uint8_t *)(currElem + 1);
        for (int x = 0; x < currElem->msg_len; ++x) {
          printf("0x%02x ", msgt[x]);
        }*/

        printf("\n");

        if (currElem->msg_type != FAPI_VENDOR_MSG_HEADER_IND) {
          switch (currElem->msg_type) {
            case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST: {
              printf("\n NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST\n");
              uint8_t * msgt = (uint8_t *)(currElem + 1);
              for (int x = 0; x < NFAPI_HEADER_LENGTH+10; ++x) {
                printf("0x%02x ", msgt[x]);
              }
              break;
            }
            case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST: {
              printf("\n NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST\n");
              uint8_t * msgt = (uint8_t *)(currElem + 1);
              for (int x = 0; x < NFAPI_HEADER_LENGTH +10; ++x) {
                printf("0x%02x ", msgt[x]);
              }
              break;
            }
          }

          procPhyMessages(currElem->msg_len, (void *)(currElem + 1), currElem->msg_type);
        }
        currElem = NULL;
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "\n[PHY] MAC2PHY WLS Get Error for msg %d\n", i);
        break;
      }
      if (flags & WLS_TF_FIN) {
        // Don't return, the messages responses will be handled in procPhyMessages
      }
    
    }
     wls_mac_add_blocks_to_ul();
  }
  return l2Msg;
}

int wls_mac_put(uint64_t pMsg, uint32_t MsgSize, uint16_t MsgTypeID, uint16_t Flags)
{
    int ret = 0;
    PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

    //printf("wls_mac_put: %p size: %d type: %d nFlags: %d\n", (void*)pMsg, MsgSize, MsgTypeID, Flags);
    //  wls_mac_show_data((void*)wls_alloc_buffer(pMsg), MsgSize);
    ret = WLS_Put(pWls->hWls, pMsg, MsgSize, MsgTypeID, Flags);

    return ret;
}


uint8_t phy_mac_send(void *msg)
{
  uint8_t ret = 0;
  uint32_t msgLen =0;
  p_fapi_api_queue_elem_t currMsg = NULL;


  PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

  if(msg)
  {
    currMsg = (p_fapi_api_queue_elem_t)msg;
    msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
    /*uint8_t * msgt = (uint8_t *)(currMsg + 1);
        for (int x = 0; x < msgLen; ++x) {
          printf("0x%02x ", msgt[x]);
        }*/

    if(currMsg->p_next == NULL)
    {
      printf("\nERROR  -->  LWR MAC : There cannot be only one block to send");
      return false;
    }
    //WLS_EnqueueBlock(g_phy_wls, WLS_VA2PA(g_phy_wls, currMsg));
    //WLS_DequeueBlock(g_phy_wls);

    /* Sending first block */
           ret = wls_mac_put(wls_mac_va_to_pa(currMsg), msgLen, currMsg->msg_type, WLS_SG_FIRST);

   // ret = WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_FIRST);
    //ret = WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_FIRST);
    if(ret != 0)
    {
      printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
      return false;
    }
    currMsg = currMsg->p_next;

    while(currMsg)
    {
      /* Sending the next msg */
      msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
      if(currMsg->p_next != NULL)
      {
        //ret = WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_NEXT);
           ret = wls_mac_put(wls_mac_va_to_pa(currMsg), msgLen, currMsg->msg_type, WLS_SG_NEXT);
        if(ret != 0)
        {
          printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
          return false;
        }
        currMsg = currMsg->p_next;
      }
      else
      {
        /* Sending last msg */
           ret = wls_mac_put(wls_mac_va_to_pa(currMsg), msgLen, currMsg->msg_type, WLS_SG_LAST);
        //ret = WLS_Put(pWls->hWls, WLS_VA2PA(pWls->hWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_LAST);
        if(ret != 0)
        {
          printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
          return false;
        }
        currMsg = NULL;
      }
    }
  }else{
    //msg doesn't exist
    printf("\nERROR  -->  LWR MAC : msg is NULL");
    return false;
  }
  printf("\ntrue  -->  LWR MAC : Message sent");
  return true;
}
