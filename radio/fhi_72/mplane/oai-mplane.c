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

#include "oai-mplane.h"
#include "common/utils/assertions.h"
/* the following line should be uncommented as it contains the config for RU
#include "openair2/GNB_APP/RRC_nr_paramsvalues.h" */

#include <assert.h>
#include <errno.h>

static FILE *fp = NULL;
static ru_config_t ru_config = {0};

int init_mplane(void)
{
  fp = popen(NETOPEER2_CLI_EXEC, "w");
  AssertFatal(fp != NULL, "Cannot open %s executable: errno %d\n", NETOPEER2_CLI_EXEC, errno);

  fprintf(fp, "auth keys add $HOME/.ssh/id_rsa.pub $HOME/.ssh/id_rsa\n");
  fprintf(fp, "listen --login root\n");

  return EXIT_SUCCESS;
}

ru_config_t get_ru_config_mplane(void)
{
  assert(fp != NULL);

  fprintf(fp, "get --filter-xpath /o-ran-delay-management:delay-management\n");
  /* 1. interact with set_fh_config() function - send the above delay params
     2. get --filter-xpath /o-ran-hardware:* - retrieve RU state (empty in VVDN)
     3. get --filter-xpath /o-ran-sync:sync
     (opt) subscribe to synchronization-state-change
     (opt) subscribe to ptp-state-change */

  return ru_config;
}

int edit_ru_config_mplane(void)
{
  /* perform CU planes configuration to the RU via 
    edit-config --target candidate --defop merge --config=<xml_file>
    validate (check if the status is "OK", if not assert)
    commit (check if the status is "OK", if not assert) */

  return EXIT_SUCCESS;
}

int free_mplane(void)
{
  assert(fp != NULL);

  fprintf(fp, "disconnect\n"); /* first unsubscribe (if subscribed to any of above).
                                  then disconnect */

  AssertFatal(pclose(fp) != -1, "Error closing netopeer2-cli: errno %d\n", errno);

  return EXIT_SUCCESS;
}
