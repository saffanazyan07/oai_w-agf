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

#include <string.h>

#include "common/utils/assertions.h"
#include "openair3/UTILS/conversions.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"

#include "e1ap_lib_common.h"
#include "e1ap_bearer_context_management.h"
#include "e1ap_lib_includes.h"

/* ====================================
 *   E1AP Bearer Context Setup Request
 * ==================================== */

/**
 * @brief Equality check for DRB_nGRAN_to_setup_t
 */
static bool eq_drb_to_setup(const DRB_nGRAN_to_setup_t *a, const DRB_nGRAN_to_setup_t *b)
{
  _E1_EQ_CHECK_LONG(a->id, b->id);
  _E1_EQ_CHECK_LONG(a->sdap_config.defaultDRB, b->sdap_config.defaultDRB);
  _E1_EQ_CHECK_LONG(a->sdap_config.sDAP_Header_UL, b->sdap_config.sDAP_Header_UL);
  _E1_EQ_CHECK_LONG(a->sdap_config.sDAP_Header_DL, b->sdap_config.sDAP_Header_DL);
  _E1_EQ_CHECK_LONG(a->pdcp_config.pDCP_SN_Size_UL, b->pdcp_config.pDCP_SN_Size_UL);
  _E1_EQ_CHECK_LONG(a->pdcp_config.pDCP_SN_Size_DL, b->pdcp_config.pDCP_SN_Size_DL);
  _E1_EQ_CHECK_LONG(a->pdcp_config.rLC_Mode, b->pdcp_config.rLC_Mode);
  _E1_EQ_CHECK_LONG(a->pdcp_config.reorderingTimer, b->pdcp_config.reorderingTimer);
  _E1_EQ_CHECK_LONG(a->pdcp_config.discardTimer, b->pdcp_config.discardTimer);
  _E1_EQ_CHECK_INT(a->pdcp_config.pDCP_Reestablishment, b->pdcp_config.pDCP_Reestablishment);
  _E1_EQ_CHECK_INT(a->tlAddress, b->tlAddress);
  _E1_EQ_CHECK_INT(a->teId, b->teId);
  _E1_EQ_CHECK_INT(a->numCellGroups, b->numCellGroups);
  if (a->numCellGroups != b->numCellGroups)
    return false;
  for (int i = 0; i < a->numCellGroups; i++) {
    _E1_EQ_CHECK_INT(a->cellGroupList[i].id, b->cellGroupList[i].id);
  }
  _E1_EQ_CHECK_INT(a->numQosFlow2Setup, b->numQosFlow2Setup);
  if (a->numQosFlow2Setup != b->numQosFlow2Setup)
    return false;
  for (int i = 0; i < a->numQosFlow2Setup; i++) {
    _E1_EQ_CHECK_LONG(a->qosFlows[i].qfi, b->qosFlows[i].qfi);
    _E1_EQ_CHECK_INT(a->qosFlows[i].qos_params.qos_characteristics.qos_type,
                     b->qosFlows[i].qos_params.qos_characteristics.qos_type);
  }
  return true;
}

/**
 * @brief Equality check for DRB_nGRAN_to_mod_t
 */
static bool eq_drb_to_mod(const DRB_nGRAN_to_mod_t *a, const DRB_nGRAN_to_mod_t *b)
{
  return eq_drb_to_setup((const DRB_nGRAN_to_setup_t *)a, (const DRB_nGRAN_to_setup_t *)b);
}

/* PDU Session To Setup Item */

static bool e1_encode_pdu_session_to_setup_item(E1AP_PDU_Session_Resource_To_Setup_Item_t *item, const pdu_session_to_setup_t *in)
{
  item->pDU_Session_ID = in->sessionId;
  item->pDU_Session_Type = in->sessionType;
  // SNSSAI
  INT8_TO_OCTET_STRING(in->nssai.sst, &item->sNSSAI.sST);
  if (in->nssai.sd != 0xffffff) {
    item->sNSSAI.sD = malloc_or_fail(sizeof(*item->sNSSAI.sD));
    INT24_TO_OCTET_STRING(in->nssai.sd, item->sNSSAI.sD);
  }
  // Security Indication
  E1AP_SecurityIndication_t *siOut = &item->securityIndication;
  const security_indication_t *siIn = &in->securityIndication;
  siOut->integrityProtectionIndication = siIn->integrityProtectionIndication;
  siOut->confidentialityProtectionIndication = siIn->confidentialityProtectionIndication;
  if (siIn->integrityProtectionIndication != E1AP_ConfidentialityProtectionIndication_not_needed) {
    asn1cCalloc(siOut->maximumIPdatarate, ipDataRate);
    ipDataRate->maxIPrate = siIn->maxIPrate;
  }
  // TNL Information
  item->nG_UL_UP_TNL_Information.present = E1AP_UP_TNL_Information_PR_gTPTunnel;
  asn1cCalloc(item->nG_UL_UP_TNL_Information.choice.gTPTunnel, gTPTunnel);
  const UP_TL_information_t *UP_TL_information = &in->UP_TL_information;
  TRANSPORT_LAYER_ADDRESS_IPv4_TO_BIT_STRING(UP_TL_information->tlAddress, &gTPTunnel->transportLayerAddress);
  INT32_TO_OCTET_STRING(UP_TL_information->teId, &gTPTunnel->gTP_TEID);
  // DRB to setup
  for (const DRB_nGRAN_to_setup_t *j = in->DRBnGRanList; j < in->DRBnGRanList + in->numDRB2Setup; j++) {
    asn1cSequenceAdd(item->dRB_To_Setup_List_NG_RAN.list, E1AP_DRB_To_Setup_Item_NG_RAN_t, ieC6_1_1);
    ieC6_1_1->dRB_ID = j->id;
    // SDAP Config
    E1AP_SDAP_Configuration_t *sdap = &ieC6_1_1->sDAP_Configuration;
    sdap->defaultDRB = j->sdap_config.defaultDRB ? E1AP_DefaultDRB_true : E1AP_DefaultDRB_false;
    sdap->sDAP_Header_UL = j->sdap_config.sDAP_Header_UL;
    sdap->sDAP_Header_DL = j->sdap_config.sDAP_Header_DL;
    // PDCP Config
    E1AP_PDCP_Configuration_t *pdcp = &ieC6_1_1->pDCP_Configuration;
    pdcp->pDCP_SN_Size_UL = j->pdcp_config.pDCP_SN_Size_UL;
    pdcp->pDCP_SN_Size_DL = j->pdcp_config.pDCP_SN_Size_DL;
    asn1cCallocOne(pdcp->discardTimer, j->pdcp_config.discardTimer);
    E1AP_T_ReorderingTimer_t *roTimer = calloc_or_fail(1, sizeof(*roTimer));
    pdcp->t_ReorderingTimer = roTimer;
    roTimer->t_Reordering = j->pdcp_config.reorderingTimer;
    pdcp->rLC_Mode = j->pdcp_config.rLC_Mode;
    // Cell Group
    for (const cell_group_t *k = j->cellGroupList; k < j->cellGroupList + j->numCellGroups; k++) {
      asn1cSequenceAdd(ieC6_1_1->cell_Group_Information.list, E1AP_Cell_Group_Information_Item_t, ieC6_1_1_1);
      ieC6_1_1_1->cell_Group_ID = k->id;
    }
    // QoS Flows
    for (const qos_flow_to_setup_t *k = j->qosFlows; k < j->qosFlows + j->numQosFlow2Setup; k++) {
      asn1cSequenceAdd(ieC6_1_1->qos_flow_Information_To_Be_Setup, E1AP_QoS_Flow_QoS_Parameter_Item_t, ieC6_1_1_1);
      ieC6_1_1_1->qoS_Flow_Identifier = k->qfi;
      // QoS Characteristics
      const qos_characteristics_t *qos_char_in = &k->qos_params.qos_characteristics;
      if (qos_char_in->qos_type == NON_DYNAMIC) { // non Dynamic 5QI
        ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.present = E1AP_QoS_Characteristics_PR_non_Dynamic_5QI;
        asn1cCalloc(ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.choice.non_Dynamic_5QI, non_Dynamic_5QI);
        non_Dynamic_5QI->fiveQI = qos_char_in->non_dynamic.fiveqi;
      } else { // dynamic 5QI
        ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.present = E1AP_QoS_Characteristics_PR_dynamic_5QI;
        asn1cCalloc(ieC6_1_1_1->qoSFlowLevelQoSParameters.qoS_Characteristics.choice.dynamic_5QI, dynamic_5QI);
        dynamic_5QI->qoSPriorityLevel = qos_char_in->dynamic.qos_priority_level;
        dynamic_5QI->packetDelayBudget = qos_char_in->dynamic.packet_delay_budget;
        dynamic_5QI->packetErrorRate.pER_Scalar = qos_char_in->dynamic.packet_error_rate.per_scalar;
        dynamic_5QI->packetErrorRate.pER_Exponent = qos_char_in->dynamic.packet_error_rate.per_exponent;
      }
      // QoS Retention Priority
      const ngran_allocation_retention_priority_t *rent_priority_in = &k->qos_params.alloc_reten_priority;
      E1AP_NGRANAllocationAndRetentionPriority_t *arp = &ieC6_1_1_1->qoSFlowLevelQoSParameters.nGRANallocationRetentionPriority;
      arp->priorityLevel = rent_priority_in->priority_level;
      arp->pre_emptionCapability = rent_priority_in->preemption_capability;
      arp->pre_emptionVulnerability = rent_priority_in->preemption_vulnerability;
    }
  }
  return true;
}

static bool e1_decode_pdu_session_to_setup_item(pdu_session_to_setup_t *out, E1AP_PDU_Session_Resource_To_Setup_Item_t *item)
{
  // PDU Session ID (M)
  out->sessionId = item->pDU_Session_ID;
  // PDU Session Type (M)
  out->sessionType = item->pDU_Session_Type;
  /* S-NSSAI (M) */
  // SST (M)
  OCTET_STRING_TO_INT8(&item->sNSSAI.sST, out->nssai.sst);
  out->nssai.sd = 0xffffff;
  // SD (O)
  if (item->sNSSAI.sD != NULL)
    OCTET_STRING_TO_INT24(item->sNSSAI.sD, out->nssai.sd);
  /* Security Indication (M) */
  E1AP_SecurityIndication_t *securityIndication = &item->securityIndication;
  // Integrity Protection Indication (M)
  security_indication_t *sec = &out->securityIndication;
  sec->integrityProtectionIndication = securityIndication->integrityProtectionIndication;
  // Confidentiality Protection Indication (M)
  sec->confidentialityProtectionIndication = securityIndication->confidentialityProtectionIndication;
  // Maximum Integrity Protected Data Rate (Conditional)
  if (sec->integrityProtectionIndication != E1AP_ConfidentialityProtectionIndication_not_needed) {
    if (securityIndication->maximumIPdatarate)
      sec->maxIPrate = securityIndication->maximumIPdatarate->maxIPrate;
    else {
      PRINT_ERROR("Received integrityProtectionIndication but maximumIPdatarate IE is missing\n");
      return false;
    }
  }
  /* NG UL UP Transport Layer Information (M) (9.3.2.1 of 3GPP TS 38.463) */
  // GTP Tunnel
  struct E1AP_GTPTunnel *gTPTunnel = item->nG_UL_UP_TNL_Information.choice.gTPTunnel;
  AssertFatal(gTPTunnel != NULL, "item->nG_UL_UP_TNL_Information.choice.gTPTunnel is a mandatory IE");
  _E1_EQ_CHECK_INT(item->nG_UL_UP_TNL_Information.present, E1AP_UP_TNL_Information_PR_gTPTunnel);
  // Transport Layer Address
  UP_TL_information_t *UP_TL_information = &out->UP_TL_information;
  BIT_STRING_TO_TRANSPORT_LAYER_ADDRESS_IPv4(&gTPTunnel->transportLayerAddress, UP_TL_information->tlAddress);
  // GTP-TEID
  OCTET_STRING_TO_INT32(&gTPTunnel->gTP_TEID, UP_TL_information->teId);
  // DRB To Setup List ( > 1 item )
  E1AP_DRB_To_Setup_List_NG_RAN_t *drb2SetupList = &item->dRB_To_Setup_List_NG_RAN;
  _E1_EQ_CHECK_GENERIC(drb2SetupList->list.count > 0, "%d", drb2SetupList->list.count, 0);
  out->numDRB2Setup = drb2SetupList->list.count;
  _E1_EQ_CHECK_INT(out->numDRB2Setup, 1); // can only handle one DRB per PDU session
  for (int j = 0; j < drb2SetupList->list.count; j++) {
    DRB_nGRAN_to_setup_t *drb = out->DRBnGRanList + j;
    E1AP_DRB_To_Setup_Item_NG_RAN_t *drb2Setup = drb2SetupList->list.array[j];
    // DRB ID (M)
    drb->id = drb2Setup->dRB_ID;
    // >SDAP Configuration (M)
    E1AP_SDAP_Configuration_t *sdap = &drb2Setup->sDAP_Configuration;
    drb->sdap_config.defaultDRB = sdap->defaultDRB == E1AP_DefaultDRB_true;
    drb->sdap_config.sDAP_Header_UL = sdap->sDAP_Header_UL;
    drb->sdap_config.sDAP_Header_DL = sdap->sDAP_Header_DL;
    // PDCP Configuration (M)
    E1AP_PDCP_Configuration_t *pdcp = &drb2Setup->pDCP_Configuration;
    drb->pdcp_config.pDCP_SN_Size_UL = pdcp->pDCP_SN_Size_UL;
    drb->pdcp_config.pDCP_SN_Size_DL = pdcp->pDCP_SN_Size_DL;
    if (pdcp->discardTimer) {
      drb->pdcp_config.discardTimer = *pdcp->discardTimer;
    }
    if (pdcp->t_ReorderingTimer) {
      drb->pdcp_config.reorderingTimer = pdcp->t_ReorderingTimer->t_Reordering;
    }
    drb->pdcp_config.rLC_Mode = pdcp->rLC_Mode;
    // Cell Group Information (M) ( > 1 item )
    E1AP_Cell_Group_Information_t *cellGroupList = &drb2Setup->cell_Group_Information;
    _E1_EQ_CHECK_GENERIC(cellGroupList->list.count > 0, "%d", cellGroupList->list.count, 0);
    drb->numCellGroups = cellGroupList->list.count;
    for (int k = 0; k < cellGroupList->list.count; k++) {
      E1AP_Cell_Group_Information_Item_t *cg2Setup = cellGroupList->list.array[k];
      // Cell Group ID
      drb->cellGroupList[k].id = cg2Setup->cell_Group_ID;
    }
    // QoS Flows Information To Be Setup (M)
    E1AP_QoS_Flow_QoS_Parameter_List_t *qos2SetupList = &drb2Setup->qos_flow_Information_To_Be_Setup;
    drb->numQosFlow2Setup = qos2SetupList->list.count;
    for (int k = 0; k < qos2SetupList->list.count; k++) {
      qos_flow_to_setup_t *qos_flow = drb->qosFlows + k;
      E1AP_QoS_Flow_QoS_Parameter_Item_t *qos2Setup = qos2SetupList->list.array[k];
      // QoS Flow Identifier (M)
      qos_flow->qfi = qos2Setup->qoS_Flow_Identifier;
      qos_characteristics_t *qos_char = &qos_flow->qos_params.qos_characteristics;
      // QoS Flow Level QoS Parameters (M)
      E1AP_QoSFlowLevelQoSParameters_t *qosParams = &qos2Setup->qoSFlowLevelQoSParameters;
      E1AP_QoS_Characteristics_t *qoS_Characteristics = &qosParams->qoS_Characteristics;
      switch (qoS_Characteristics->present) {
        case E1AP_QoS_Characteristics_PR_non_Dynamic_5QI:
          qos_char->qos_type = NON_DYNAMIC;
          qos_char->non_dynamic.fiveqi = qoS_Characteristics->choice.non_Dynamic_5QI->fiveQI;
          break;
        case E1AP_QoS_Characteristics_PR_dynamic_5QI: {
          E1AP_Dynamic5QIDescriptor_t *dynamic5QI = qoS_Characteristics->choice.dynamic_5QI;
          qos_char->qos_type = DYNAMIC;
          qos_char->dynamic.qos_priority_level = dynamic5QI->qoSPriorityLevel;
          qos_char->dynamic.packet_delay_budget = dynamic5QI->packetDelayBudget;
          qos_char->dynamic.packet_error_rate.per_scalar = dynamic5QI->packetErrorRate.pER_Scalar;
          qos_char->dynamic.packet_error_rate.per_exponent = dynamic5QI->packetErrorRate.pER_Exponent;
          break;
        }
        default:
          PRINT_ERROR("Unexpected QoS Characteristics type: %d\n", qoS_Characteristics->present);
          break;
      }
      // NG-RAN Allocation and Retention Priority (M)
      ngran_allocation_retention_priority_t *rent_priority = &qos_flow->qos_params.alloc_reten_priority;
      E1AP_NGRANAllocationAndRetentionPriority_t *aRP = &qosParams->nGRANallocationRetentionPriority;
      rent_priority->priority_level = aRP->priorityLevel;
      rent_priority->preemption_capability = aRP->pre_emptionCapability;
      rent_priority->preemption_vulnerability = aRP->pre_emptionVulnerability;
    }
  }
  return true;
}

/**
 * @brief Equality check for PDU session item to modify/setup
 */
static bool eq_pdu_session_item(const pdu_session_to_setup_t *a, const pdu_session_to_setup_t *b)
{
  _E1_EQ_CHECK_LONG(a->sessionId, b->sessionId);
  _E1_EQ_CHECK_LONG(a->sessionType, b->sessionType);
  _E1_EQ_CHECK_INT(a->UP_TL_information.tlAddress, b->UP_TL_information.tlAddress);
  _E1_EQ_CHECK_INT(a->UP_TL_information.teId, b->UP_TL_information.teId);
  _E1_EQ_CHECK_LONG(a->numDRB2Setup, b->numDRB2Setup);
  if (a->numDRB2Setup != b->numDRB2Setup)
    return false;
  for (int i = 0; i < a->numDRB2Setup; i++)
    if (!eq_drb_to_setup(&a->DRBnGRanList[i], &b->DRBnGRanList[i]))
      return false;
  _E1_EQ_CHECK_LONG(a->numDRB2Modify, b->numDRB2Modify);
  if (a->numDRB2Modify != b->numDRB2Modify)
    return false;
  for (int i = 0; i < a->numDRB2Modify; i++)
    if (!eq_drb_to_mod(&a->DRBnGRanModList[i], &b->DRBnGRanModList[i]))
      return false;
  return true;
}

static bool cp_pdu_session_item(pdu_session_to_setup_t *cp, const pdu_session_to_setup_t *msg)
{
  // Copy basic fields
  *cp = *msg;
  // Copy DRB to Setup List
  for (int j = 0; j < msg->numDRB2Setup; j++) {
    cp->DRBnGRanList[j].id = msg->DRBnGRanList[j].id;
    cp->DRBnGRanList[j].sdap_config = msg->DRBnGRanList[j].sdap_config;
    cp->DRBnGRanList[j].pdcp_config = msg->DRBnGRanList[j].pdcp_config;
    cp->DRBnGRanList[j].tlAddress = msg->DRBnGRanList[j].tlAddress;
    cp->DRBnGRanList[j].teId = msg->DRBnGRanList[j].teId;
    cp->DRBnGRanList[j].numDlUpParam = msg->DRBnGRanList[j].numDlUpParam;
    // Copy DL UP Params List
    for (int k = 0; k < msg->DRBnGRanList[j].numDlUpParam; k++) {
      cp->DRBnGRanList[j].DlUpParamList[k] = msg->DRBnGRanList[j].DlUpParamList[k];
    }
    cp->DRBnGRanList[j].numCellGroups = msg->DRBnGRanList[j].numCellGroups;
    // Copy Cell Group List
    for (int k = 0; k < msg->DRBnGRanList[j].numCellGroups; k++) {
      cp->DRBnGRanList[j].cellGroupList[k] = msg->DRBnGRanList[j].cellGroupList[k];
    }
    cp->DRBnGRanList[j].numQosFlow2Setup = msg->DRBnGRanList[j].numQosFlow2Setup;
    // Copy QoS Flow To Setup List
    for (int k = 0; k < msg->DRBnGRanList[j].numQosFlow2Setup; k++) {
      cp->DRBnGRanList[j].qosFlows[k].qfi = msg->DRBnGRanList[j].qosFlows[k].qfi;
      cp->DRBnGRanList[j].qosFlows[k].qos_params = msg->DRBnGRanList[j].qosFlows[k].qos_params;
    }
  }
  // Copy DRB to Modify List
  for (int j = 0; j < msg->numDRB2Modify; j++) {
    cp->DRBnGRanModList[j] = msg->DRBnGRanModList[j];
  }
  return true;
}

/**
 * @brief E1AP Bearer Context Setup Request encoding (9.2.2 of 3GPP TS 38.463)
 */
E1AP_E1AP_PDU_t *encode_E1_bearer_context_setup_request(const e1ap_bearer_setup_req_t *msg)
{
  E1AP_E1AP_PDU_t *pdu = calloc_or_fail(1, sizeof(*pdu));

  pdu->present = E1AP_E1AP_PDU_PR_initiatingMessage;
  asn1cCalloc(pdu->choice.initiatingMessage, initMsg);
  initMsg->procedureCode = E1AP_ProcedureCode_id_bearerContextSetup;
  initMsg->criticality = E1AP_Criticality_reject;
  initMsg->value.present = E1AP_InitiatingMessage__value_PR_BearerContextSetupRequest;
  E1AP_BearerContextSetupRequest_t *out = &pdu->choice.initiatingMessage->value.choice.BearerContextSetupRequest;
  /* mandatory */
  /* c1. gNB-CU-UP UE E1AP ID */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC1);
  ieC1->id = E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID;
  ieC1->criticality = E1AP_Criticality_reject;
  ieC1->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID;
  ieC1->value.choice.GNB_CU_CP_UE_E1AP_ID = msg->gNB_cu_cp_ue_id;
  /* mandatory */
  /* c2. Security Information */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC2);
  ieC2->id = E1AP_ProtocolIE_ID_id_SecurityInformation;
  ieC2->criticality = E1AP_Criticality_reject;
  ieC2->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_SecurityInformation;
  E1AP_SecurityAlgorithm_t *securityAlgorithm = &ieC2->value.choice.SecurityInformation.securityAlgorithm;
  E1AP_UPSecuritykey_t *uPSecuritykey = &ieC2->value.choice.SecurityInformation.uPSecuritykey;
  securityAlgorithm->cipheringAlgorithm = msg->cipheringAlgorithm;
  OCTET_STRING_fromBuf(&uPSecuritykey->encryptionKey, msg->encryptionKey, E1AP_SECURITY_KEY_SIZE);
  asn1cCallocOne(securityAlgorithm->integrityProtectionAlgorithm, msg->integrityProtectionAlgorithm);
  asn1cCalloc(uPSecuritykey->integrityProtectionKey, protKey);
  OCTET_STRING_fromBuf(protKey, msg->integrityProtectionKey, E1AP_SECURITY_KEY_SIZE);
  /* mandatory */
  /* c3. UE DL Aggregate Maximum Bit Rate */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC3);
  ieC3->id = E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate;
  ieC3->criticality = E1AP_Criticality_reject;
  ieC3->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_BitRate;
  asn_long2INTEGER(&ieC3->value.choice.BitRate, msg->ueDlAggMaxBitRate);
  /* mandatory */
  /* c4. Serving PLMN */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC4);
  ieC4->id = E1AP_ProtocolIE_ID_id_Serving_PLMN;
  ieC4->criticality = E1AP_Criticality_ignore;
  ieC4->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_PLMN_Identity;
  const PLMN_ID_t *servingPLMN = &msg->servingPLMNid;
  MCC_MNC_TO_PLMNID(servingPLMN->mcc, servingPLMN->mnc, servingPLMN->mnc_digit_length, &ieC4->value.choice.PLMN_Identity);
  /* mandatory */
  /* Activity Notification Level */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC5);
  ieC5->id = E1AP_ProtocolIE_ID_id_ActivityNotificationLevel;
  ieC5->criticality = E1AP_Criticality_reject;
  ieC5->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel;
  ieC5->value.choice.ActivityNotificationLevel = E1AP_ActivityNotificationLevel_pdu_session; // TODO: Remove hard coding
  /* mandatory */
  /*  */
  asn1cSequenceAdd(out->protocolIEs.list, E1AP_BearerContextSetupRequestIEs_t, ieC6);
  ieC6->id = E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest;
  ieC6->criticality = E1AP_Criticality_reject;
  ieC6->value.present = E1AP_BearerContextSetupRequestIEs__value_PR_System_BearerContextSetupRequest;
  ieC6->value.choice.System_BearerContextSetupRequest.present =
      E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest;
  E1AP_ProtocolIE_Container_4932P19_t *msgNGRAN_list = calloc_or_fail(1, sizeof(*msgNGRAN_list));
  ieC6->value.choice.System_BearerContextSetupRequest.choice.nG_RAN_BearerContextSetupRequest =
      (struct E1AP_ProtocolIE_Container *)msgNGRAN_list;
  asn1cSequenceAdd(msgNGRAN_list->list, E1AP_NG_RAN_BearerContextSetupRequest_t, msgNGRAN);
  msgNGRAN->id = E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Setup_List;
  msgNGRAN->criticality = E1AP_Criticality_reject;
  msgNGRAN->value.present = E1AP_NG_RAN_BearerContextSetupRequest__value_PR_PDU_Session_Resource_To_Setup_List;
  E1AP_PDU_Session_Resource_To_Setup_List_t *pdu2Setup = &msgNGRAN->value.choice.PDU_Session_Resource_To_Setup_List;
  for (int i = 0; i < msg->numPDUSessions; i++) {
    const pdu_session_to_setup_t *pdu_session = &msg->pduSession[i];
    asn1cSequenceAdd(pdu2Setup->list, E1AP_PDU_Session_Resource_To_Setup_Item_t, ieC6_1);
    e1_encode_pdu_session_to_setup_item(ieC6_1, pdu_session);
  }
  return pdu;
}

/**
 * @brief E1AP Bearer Context Setup Request memory management
 */
void free_e1ap_context_setup_request(e1ap_bearer_setup_req_t *msg)
{
  // Do nothing
}

/**
 * @brief E1AP Bearer Context Setup Request decoding
 *        gNB-CU-CP → gNB-CU-UP (9.2.2.1 of 3GPP TS 38.463)
 */
bool decode_E1_bearer_context_setup_request(const E1AP_E1AP_PDU_t *pdu, e1ap_bearer_setup_req_t *out)
{
  const E1AP_BearerContextSetupRequest_t *in = &pdu->choice.initiatingMessage->value.choice.BearerContextSetupRequest;
  E1AP_BearerContextSetupRequestIEs_t *ie;
  // Check mandatory IEs first
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_SecurityInformation, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_Serving_PLMN, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_ActivityNotificationLevel, true);
  E1AP_LIB_FIND_IE(E1AP_BearerContextSetupRequestIEs_t, ie, in, E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest, true);
  // Loop over all IEs
  for (int i = 0; i < in->protocolIEs.list.count; i++) {
    ie = in->protocolIEs.list.array[i];
    AssertFatal(ie != NULL, "in->protocolIEs.list.array[i] shall not be null");
    switch (ie->id) {
      case E1AP_ProtocolIE_ID_id_gNB_CU_CP_UE_E1AP_ID:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_GNB_CU_CP_UE_E1AP_ID);
        out->gNB_cu_cp_ue_id = ie->value.choice.GNB_CU_CP_UE_E1AP_ID;
        break;

      case E1AP_ProtocolIE_ID_id_SecurityInformation:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_SecurityInformation);
        E1AP_SecurityInformation_t *SecurityInformation = &ie->value.choice.SecurityInformation;
        E1AP_SecurityAlgorithm_t *securityAlgorithm = &SecurityInformation->securityAlgorithm;
        E1AP_EncryptionKey_t *encryptionKey = &SecurityInformation->uPSecuritykey.encryptionKey;
        E1AP_IntegrityProtectionKey_t *integrityProtectionKey = SecurityInformation->uPSecuritykey.integrityProtectionKey;

        out->cipheringAlgorithm = securityAlgorithm->cipheringAlgorithm;
        memcpy(out->encryptionKey, encryptionKey->buf, encryptionKey->size);
        if (securityAlgorithm->integrityProtectionAlgorithm)
          out->integrityProtectionAlgorithm = *securityAlgorithm->integrityProtectionAlgorithm;
        if (integrityProtectionKey)
          memcpy(out->integrityProtectionKey, integrityProtectionKey->buf, integrityProtectionKey->size);
        break;

      case E1AP_ProtocolIE_ID_id_UEDLAggregateMaximumBitRate:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_BitRate);
        asn_INTEGER2long(&ie->value.choice.BitRate, &out->ueDlAggMaxBitRate);
        break;

      case E1AP_ProtocolIE_ID_id_Serving_PLMN:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_PLMN_Identity);
        PLMNID_TO_MCC_MNC(&ie->value.choice.PLMN_Identity,
                          out->servingPLMNid.mcc,
                          out->servingPLMNid.mnc,
                          out->servingPLMNid.mnc_digit_length);
        break;

      case E1AP_ProtocolIE_ID_id_ActivityNotificationLevel:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel);
        DevAssert(ie->value.present == E1AP_BearerContextSetupRequestIEs__value_PR_ActivityNotificationLevel);
        out->activityNotificationLevel = ie->value.choice.ActivityNotificationLevel;
        break;

      case E1AP_ProtocolIE_ID_id_System_BearerContextSetupRequest:
        _E1_EQ_CHECK_INT(ie->value.present, E1AP_BearerContextSetupRequestIEs__value_PR_System_BearerContextSetupRequest);
        E1AP_System_BearerContextSetupRequest_t *System_BearerContextSetupRequest =
            &ie->value.choice.System_BearerContextSetupRequest;
        switch (System_BearerContextSetupRequest->present) {
          case E1AP_System_BearerContextSetupRequest_PR_NOTHING:
            PRINT_ERROR("System Bearer Context Setup Request: no choice present\n");
            break;
          case E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest:
            AssertError(
                System_BearerContextSetupRequest->present
                    == E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest,
                return false,
                "E1AP_System_BearerContextSetupRequest_PR_e_UTRAN_BearerContextSetupRequest in E1 Setup Request not supported");
            break;
          case E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest:
            // Handle nG-RAN Bearer Context Setup Request
            _E1_EQ_CHECK_INT(System_BearerContextSetupRequest->present,
                             E1AP_System_BearerContextSetupRequest_PR_nG_RAN_BearerContextSetupRequest);
            AssertFatal(System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest != NULL,
                        "System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest shall not be null");
            break;
          default:
            PRINT_ERROR("System Bearer Context Setup Request: No valid choice present.\n");
            break;
        }

        E1AP_ProtocolIE_Container_4932P19_t *msgNGRAN_list =
            (E1AP_ProtocolIE_Container_4932P19_t *)System_BearerContextSetupRequest->choice.nG_RAN_BearerContextSetupRequest;
        E1AP_NG_RAN_BearerContextSetupRequest_t *msgNGRAN = msgNGRAN_list->list.array[0];
        // NG-RAN
        _E1_EQ_CHECK_INT(msgNGRAN_list->list.count, 1); // only one RAN is expected
        _E1_EQ_CHECK_LONG(msgNGRAN->id, E1AP_ProtocolIE_ID_id_PDU_Session_Resource_To_Setup_List);
        _E1_EQ_CHECK_INT(msgNGRAN->value.present,
                         E1AP_NG_RAN_BearerContextSetupRequest__value_PR_PDU_Session_Resource_To_Setup_List);
        // PDU Session Resource To Setup List (9.3.3.2 of 3GPP TS 38.463)
        E1AP_PDU_Session_Resource_To_Setup_List_t *pdu2SetupList = &msgNGRAN->value.choice.PDU_Session_Resource_To_Setup_List;
        out->numPDUSessions = pdu2SetupList->list.count;
        // Loop through all PDU sessions
        for (int i = 0; i < pdu2SetupList->list.count; i++) {
          pdu_session_to_setup_t *pdu_session = out->pduSession + i;
          E1AP_PDU_Session_Resource_To_Setup_Item_t *pdu2Setup = pdu2SetupList->list.array[i];
          e1_decode_pdu_session_to_setup_item(pdu_session, pdu2Setup);
        }
        break;
      default:
        PRINT_ERROR("Handle for this IE %ld is not implemented (or) invalid IE detected\n", ie->id);
        break;
    }
  }
  return true;
}

/**
 * @brief Deep copy function for E1 BEARER CONTEXT SETUP REQUEST
 */
e1ap_bearer_setup_req_t cp_bearer_context_setup_request(const e1ap_bearer_setup_req_t *msg)
{
  e1ap_bearer_setup_req_t cp = {0};
  // Copy basi fields
  cp = *msg;
  strncpy(cp.encryptionKey, msg->encryptionKey, sizeof(cp.encryptionKey));
  strncpy(cp.integrityProtectionKey, msg->integrityProtectionKey, sizeof(cp.integrityProtectionKey));
  // Copy PDU Sessions
  for (int i = 0; i < msg->numPDUSessions; i++)
    cp_pdu_session_item(&cp.pduSession[i], &msg->pduSession[i]);
  return cp;
}

/**
 * @brief E1AP Bearer Context Setup Request equality check
 */
bool eq_bearer_context_setup_request(const e1ap_bearer_setup_req_t *a, const e1ap_bearer_setup_req_t *b)
{
  // Primitive data types
  _E1_EQ_CHECK_INT(a->gNB_cu_cp_ue_id, b->gNB_cu_cp_ue_id);
  _E1_EQ_CHECK_LONG(a->cipheringAlgorithm, b->cipheringAlgorithm);
  _E1_EQ_CHECK_LONG(a->integrityProtectionAlgorithm, b->integrityProtectionAlgorithm);
  _E1_EQ_CHECK_LONG(a->ueDlAggMaxBitRate, b->ueDlAggMaxBitRate);
  _E1_EQ_CHECK_INT(a->activityNotificationLevel, b->activityNotificationLevel);
  _E1_EQ_CHECK_INT(a->numPDUSessions, b->numPDUSessions);
  _E1_EQ_CHECK_INT(a->numPDUSessionsMod, b->numPDUSessionsMod);
  // PLMN
  _E1_EQ_CHECK_INT(a->servingPLMNid.mcc, b->servingPLMNid.mcc);
  _E1_EQ_CHECK_INT(a->servingPLMNid.mnc, b->servingPLMNid.mnc);
  _E1_EQ_CHECK_INT(a->servingPLMNid.mnc_digit_length, b->servingPLMNid.mnc_digit_length);
  // Security Keys
  _E1_EQ_CHECK_STR(a->encryptionKey, b->encryptionKey);
  _E1_EQ_CHECK_STR(a->integrityProtectionKey, b->integrityProtectionKey);
  // PDU Sessions
  if (a->numPDUSessions != b->numPDUSessions)
    return false;
  for (int i = 0; i < a->numPDUSessions; i++)
    if (!eq_pdu_session_item(&a->pduSession[i], &b->pduSession[i]))
      return false;
  if (a->numPDUSessionsMod != b->numPDUSessionsMod)
    return false;
  for (int i = 0; i < a->numPDUSessionsMod; i++)
    if (!eq_pdu_session_item(&a->pduSessionMod[i], &b->pduSessionMod[i]))
      return false;
  return true;
}
