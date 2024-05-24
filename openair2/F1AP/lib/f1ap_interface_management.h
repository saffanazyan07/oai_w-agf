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

#ifndef F1AP_INTERFACE_MANAGEMENT_H_
#define F1AP_INTERFACE_MANAGEMENT_H_

#include <stdbool.h>
#include "f1ap_messages_types.h"

struct F1AP_F1AP_PDU;

struct F1AP_F1AP_PDU *encode_f1ap_setup_request(const f1ap_setup_req_t *msg);
bool decode_f1ap_setup_request(const struct F1AP_F1AP_PDU *pdu, f1ap_setup_req_t *out);
f1ap_setup_req_t cp_f1ap_setup_request(const f1ap_setup_req_t *msg);
bool eq_f1ap_setup_request(const f1ap_setup_req_t *a, const f1ap_setup_req_t *b);
void free_f1ap_setup_request(f1ap_setup_req_t *msg);

/* F1 Setup Response */
struct F1AP_F1AP_PDU *encode_f1ap_setup_response(const f1ap_setup_resp_t *msg);
bool decode_f1ap_setup_response(const struct F1AP_F1AP_PDU *pdu, f1ap_setup_resp_t *out);
void free_f1ap_setup_response(f1ap_setup_resp_t *msg);
f1ap_setup_resp_t cp_f1ap_setup_response(const f1ap_setup_resp_t *msg);
bool eq_f1ap_setup_response(const f1ap_setup_resp_t *a, const f1ap_setup_resp_t *b);

#endif /* F1AP_INTERFACE_MANAGEMENT_H_ */