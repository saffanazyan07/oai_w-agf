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
#include "wls_vnf.h"
uint64_t        nWlsMacMemorySize = 0;
uint64_t        nWlsPhyMemorySize = 0;
static nfapi_vnf_config_t *config;
static WLS_MAC_CTX wls_mac_iface;
static PWLS_MAC_CTX wls_mac_get_ctx(void)
{
  return &wls_mac_iface;
}

static int vnf_wls_init()
{
  uint8_t i;
  void *hdl = NULL;
  PWLS_MAC_CTX pWls =  wls_mac_get_ctx();

  char *my_argv[] = {"gnodeb", "-c3", "--proc-type=auto", "--file-prefix", WLS_DEV_NAME, "--iova-mode=pa"};
  printf("\nCalling rte_eal_init: ");
  for (i = 0; i < RTE_DIM(my_argv); i++)
  {
    printf("%s ", my_argv[i]);
  }
  printf("\n");

  if (rte_eal_init(RTE_DIM(my_argv), my_argv) < 0)
    rte_panic("\nCannot init EAL\n");


  hdl = WLS_Open(WLS_DEV_NAME, WLS_MASTER_CLIENT, &nWlsMacMemorySize, &nWlsPhyMemorySize);
  if(hdl == NULL)
  {
    printf("\nERROR: WLS_Open > DEVICE_NAME mismatch. WLS Device Name should be same as 'wls_dev_name' parameter in 'phycfg_xran.xml' file");
  }

  pWls->hWls = hdl;

  if(!pWls->hWls)
  {
    printf("\nCould not open WLS Interface \n");
    return 0;
  }

  return 1;
}

void *wls_fapi_vnf_nr_start_thread(void *ptr)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] IN WLS PNF NFAPI start thread %s\n", __FUNCTION__);
  //pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
  wls_fapi_nr_vnf_start((nfapi_vnf_config_t *)ptr);
  return (void *)0;
}

int wls_fapi_nr_vnf_start(nfapi_vnf_config_t *cfg){
  config = cfg;

  if(config == 0) {
    return -1;
  }
  vnf_t* vnf = (vnf_t*)(config);

  // init WLS connection
  if(vnf_wls_init() == 0) {
    return -1;
  }
vnf->terminate = 0;




  return 1;
}
