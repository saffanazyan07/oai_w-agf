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

#include "wls_common.h"

uint8_t wls_msg_send(PWLS_MAC_CTX pWls, void *msg)
{
  uint32_t msgLen = 0;
  p_fapi_api_queue_elem_t currMsg = NULL;

  if (msg) {
    currMsg = (p_fapi_api_queue_elem_t)msg;
    msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
    if (currMsg->p_next == NULL) {
      printf("\nERROR  -->  LWR MAC : There cannot be only one block to send");
      return false;
    }
    /* Sending first block */
    /* Call WLS_VA2PA to convert struct pointer to physical address */
    if (WLS_Put(pWls, WLS_VA2PA(pWls, currMsg), msgLen, currMsg->msg_type, WLS_SG_FIRST) != 0) {
      printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
      return false;
    }
    /* Advance to the next message block */
    currMsg = currMsg->p_next;
    while (currMsg) {
      /* Sending the next msg */
      msgLen = currMsg->msg_len + sizeof(fapi_api_queue_elem_t);
      /* Send the block with appropriate flags */
      const unsigned short flags = currMsg->p_next != NULL ? WLS_SG_NEXT : WLS_SG_LAST;
      if (WLS_Put(pWls, WLS_VA2PA(pWls, currMsg), msgLen, currMsg->msg_type, flags) != 0) {
        printf("\nERROR  -->  LWR MAC : Failure in sending message to PHY");
        return false;
      }
      /* No need for ternary operator, if p_next is NULL, that's what we want */
      /* currMsg = currMsg->p_next == NULL ? NULL : currMsg->p_next;*/
      currMsg = currMsg->p_next;
    }
  } else {
    //msg doesn't exist
    printf("\nERROR  -->  LWR MAC : msg is NULL");
    return false;
  }
  return true;
}
