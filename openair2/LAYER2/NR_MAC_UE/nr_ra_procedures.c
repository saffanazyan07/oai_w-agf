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

/*! \file ra_procedures.c
 * \brief Routines for UE MAC-layer Random Access procedures (TS 38.321, Release 15)
 * \author R. Knopp, Navid Nikaein, Guido Casati
 * \date 2019
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr navid.nikaein@eurecom.fr, guido.casati@iis.fraunhofer.de
 * \note
 * \warning
 */

/* RRC */
#include "RRC/NR_UE/L2_interface_ue.h"

/* MAC */
#include "LAYER2/NR_MAC_COMMON/nr_mac_extern.h"
#include "NR_MAC_COMMON/nr_mac.h"
#include "LAYER2/NR_MAC_UE/mac_proto.h"

#include <executables/softmodem-common.h>
#include "openair2/LAYER2/RLC/rlc.h"

static void nr_get_prach_resources(NR_UE_MAC_INST_t *mac,
                                   NR_PRACH_RESOURCES_t *prach_resources,
                                   NR_RACH_ConfigDedicated_t *rach_ConfigDedicated);

static double get_ta_Common_ms(NR_NTN_Config_r17_t *ntn_Config_r17)
{
  if (ntn_Config_r17 && ntn_Config_r17->ta_Info_r17)
    return ntn_Config_r17->ta_Info_r17->ta_Common_r17 * 4.072e-6; // ta_Common_r17 is in units of 4.072e-3 µs
  return 0.0;
}

int16_t get_prach_tx_power(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;
  int16_t pathloss = compute_nr_SSB_PL(mac, mac->ssb_measurements.ssb_rsrp_dBm);
  int16_t ra_preamble_rx_power = (int16_t)(ra->prach_resources.ra_PREAMBLE_RECEIVED_TARGET_POWER + pathloss);
  return min(ra->prach_resources.Pc_max, ra_preamble_rx_power);
}

static void set_preambleTransMax(RA_config_t *ra, long preambleTransMax)
{
  switch (preambleTransMax) {
    case 0:
      ra->preambleTransMax = 3;
      break;
    case 1:
      ra->preambleTransMax = 4;
      break;
    case 2:
      ra->preambleTransMax = 5;
      break;
    case 3:
      ra->preambleTransMax = 6;
      break;
    case 4:
      ra->preambleTransMax = 7;
      break;
    case 5:
      ra->preambleTransMax = 8;
      break;
    case 6:
      ra->preambleTransMax = 10;
      break;
    case 7:
      ra->preambleTransMax = 20;
      break;
    case 8:
      ra->preambleTransMax = 50;
      break;
    case 9:
      ra->preambleTransMax = 100;
      break;
    case 10:
      ra->preambleTransMax = 200;
      break;
    default:
      AssertFatal(false, "Invalid preambleTransMax\n");
  }
}

static int get_Msg3SizeGroupA(long ra_Msg3SizeGroupA)
{
  int bits = 0;
  switch (ra_Msg3SizeGroupA) {
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b56:
      bits = 56;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b144:
      bits = 144;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b208:
      bits = 208;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b256:
      bits = 256;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b282:
      bits = 282;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b480:
      bits = 480;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b640:
      bits = 640;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b800:
      bits = 800;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b1000:
      bits = 1000;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__ra_Msg3SizeGroupA_b72:
      bits = 72;
      break;
    default:
      AssertFatal(false, "Unknown ra-Msg3SizeGroupA %lu\n", ra_Msg3SizeGroupA);
  }
  return bits / 8; // returning bytes
}

static int get_messagePowerOffsetGroupB(long messagePowerOffsetGroupB)
{
  int pow_offset = 0;
  switch (messagePowerOffsetGroupB) {
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_minusinfinity:
      pow_offset = INT_MIN;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB0:
      pow_offset = 0;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB5:
      pow_offset = 5;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB8:
      pow_offset = 8;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB10:
      pow_offset = 10;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB12:
      pow_offset = 12;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB15:
      pow_offset = 15;
      break;
    case NR_RACH_ConfigCommon__groupBconfigured__messagePowerOffsetGroupB_dB18:
      pow_offset = 18;
      break;
    default:
      AssertFatal(false,"Unknown messagePowerOffsetGroupB %lu\n", messagePowerOffsetGroupB);
  }
  return pow_offset;
}

static void select_preamble_group(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;
  // TODO if the RA_TYPE is switched from 2-stepRA to 4-stepRA

  if (!ra->Msg3_buffer) { // if Msg3 buffer is empty
    NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = mac->current_UL_BWP->rach_ConfigCommon;
    if (nr_rach_ConfigCommon && nr_rach_ConfigCommon->groupBconfigured) { // if Random Access Preambles group B is configured
      struct NR_RACH_ConfigCommon__groupBconfigured *groupB = nr_rach_ConfigCommon->groupBconfigured;
      // if the potential Msg3 size (UL data available for transmission plus MAC subheader(s) and,
      // where required, MAC CEs) is greater than ra-Msg3SizeGroupA
      // if the pathloss is less than PCMAX (of the Serving Cell performing the Random Access Procedure)
      // – preambleReceivedTargetPower – msg3-DeltaPreamble – messagePowerOffsetGroupB
      int groupB_pow_offset = get_messagePowerOffsetGroupB(groupB->messagePowerOffsetGroupB);
      int PLThreshold = ra->prach_resources.Pc_max - ra->preambleRxTargetPower - ra->deltaPreamble - groupB_pow_offset;
      int pathloss = compute_nr_SSB_PL(mac, mac->ssb_measurements.ssb_rsrp_dBm);
      // TODO if the Random Access procedure was initiated for the CCCH logical channel and the CCCH SDU size
      // plus MAC subheader is greater than ra-Msg3SizeGroupA
      if (ra->Msg3_size > get_Msg3SizeGroupA(groupB->ra_Msg3SizeGroupA) && pathloss < PLThreshold)
        ra->RA_GroupA = false;
      else
        ra->RA_GroupA = true;
    } else
      ra->RA_GroupA = true;
  }
  // else if Msg3 is being retransmitted, we keep what used in first transmission of Msg3
}

typedef struct{
  float ssb_per_ro;
  int preambles_per_ssb;
} ssb_ro_preambles_t;

static ssb_ro_preambles_t get_ssb_ro_preambles_4step(struct NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB *config)
{
  ssb_ro_preambles_t ret = {0};
  switch (config->present) {
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneEighth :
      ret.ssb_per_ro = 1 / 8;
      ret.preambles_per_ssb = (config->choice.oneEighth + 1) << 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneFourth :
      ret.ssb_per_ro = 1 / 4;
      ret.preambles_per_ssb = (config->choice.oneFourth + 1) << 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_oneHalf :
      ret.ssb_per_ro = 1 / 2;
      ret.preambles_per_ssb = (config->choice.oneHalf + 1) << 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_one :
      ret.ssb_per_ro = 1;
      ret.preambles_per_ssb = (config->choice.one + 1) << 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_two :
      ret.ssb_per_ro = 2;
      ret.preambles_per_ssb = (config->choice.two + 1) << 2;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_four :
      ret.ssb_per_ro = 4;
      ret.preambles_per_ssb = config->choice.four;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_eight :
      ret.ssb_per_ro = 8;
      ret.preambles_per_ssb = config->choice.eight;
      break;
    case NR_RACH_ConfigCommon__ssb_perRACH_OccasionAndCB_PreamblesPerSSB_PR_sixteen :
      ret.ssb_per_ro = 16;
      ret.preambles_per_ssb = config->choice.sixteen;
      break;
    default :
      AssertFatal(false, "Invalid ssb_perRACH_OccasionAndCB_PreamblesPerSSB\n");
  }
  return ret;
}

static ssb_ro_preambles_t get_ssb_ro_preambles_2step(struct NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16 *config)
{
  ssb_ro_preambles_t ret = {0};
  switch (config->present) {
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_oneEighth :
      ret.ssb_per_ro = 1 / 8;
      ret.preambles_per_ssb = (config->choice.oneEighth + 1) << 2;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_oneFourth :
      ret.ssb_per_ro = 1 / 4;
      ret.preambles_per_ssb = (config->choice.oneFourth + 1) << 2;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_oneHalf :
      ret.ssb_per_ro = 1 / 2;
      ret.preambles_per_ssb = (config->choice.oneHalf + 1) << 2;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_one :
      ret.ssb_per_ro = 1;
      ret.preambles_per_ssb = (config->choice.one + 1) << 2;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_two :
      ret.ssb_per_ro = 2;
      ret.preambles_per_ssb = (config->choice.two + 1) << 2;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_four :
      ret.ssb_per_ro = 4;
      ret.preambles_per_ssb = config->choice.four;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_eight :
      ret.ssb_per_ro = 8;
      ret.preambles_per_ssb = config->choice.eight;
      break;
    case NR_RACH_ConfigCommonTwoStepRA_r16__msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16_PR_sixteen :
      ret.ssb_per_ro = 16;
      ret.preambles_per_ssb = config->choice.sixteen;
      break;
    default :
      AssertFatal(false, "Invalid msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16\n");
  }
  return ret;
}

static void config_preamble_index(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;
  // Random seed generation
  unsigned int seed;
  if (IS_SOFTMODEM_IQPLAYER || IS_SOFTMODEM_IQRECORDER) {
    // Overwrite seed with non-random seed for IQ player/recorder
    seed = 1;
  } else {
    // & to truncate the int64_t and keep only the LSB bits, up to sizeof(int)
    seed = (unsigned int) (rdtsc_oai() & ~0);
  }

  NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = mac->current_UL_BWP->rach_ConfigCommon;
  ssb_ro_preambles_t config_info;
  int nb_of_preambles = 64;
  bool groupBconfigured = false;
  int preamb_ga = 0;
  if (ra->ra_type == RA_4_STEP) {
    AssertFatal(nr_rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB,
                "Not expeting ssb_perRACH_OccasionAndCB_PreamblesPerSSB to be NULL here\n");
    config_info = get_ssb_ro_preambles_4step(nr_rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB);
    if (nr_rach_ConfigCommon->totalNumberOfRA_Preambles)
      nb_of_preambles = *nr_rach_ConfigCommon->totalNumberOfRA_Preambles;
    // Amongst the contention-based Random Access Preambles associated with an SSB the first numberOfRA-PreamblesGroupA
    // included in groupBconfigured Random Access Preambles belong to Random Access Preambles group A.
    // The remaining Random Access Preambles associated with the SSB belong to Random Access Preambles group B (if configured)
    select_preamble_group(mac);
    if (nr_rach_ConfigCommon->groupBconfigured) {
      groupBconfigured = true;
      preamb_ga = nr_rach_ConfigCommon->groupBconfigured->numberOfRA_PreamblesGroupA;
    }
  } else {
    NR_RACH_ConfigCommonTwoStepRA_r16_t *twostep = &mac->current_UL_BWP->msgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16;
    AssertFatal(twostep->groupB_ConfiguredTwoStepRA_r16 == NULL, "GroupB preambles not supported for 2-step RA\n");
    ra->RA_GroupA = true;
    // The field is mandatory present if the 2-step random access type occasions are shared with 4-step random access type,
    // otherwise the field is not present
    bool sharedROs = twostep->msgA_CB_PreamblesPerSSB_PerSharedRO_r16 != NULL;
    AssertFatal(sharedROs == false, "Shared ROs between 2- and 4-step RA not supported\n");

    // For Type-2 random access procedure with separate configuration of PRACH occasions with Type-1 random access procedure
    // configuration by msgA-SSB-PerRACH-OccasionAndCB-PreamblesPerSSB when provided;
    // otherwise, by ssb-perRACH-OccasionAndCB-PreamblesPerSSB
    if (twostep->msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16)
      config_info = get_ssb_ro_preambles_2step(twostep->msgA_SSB_PerRACH_OccasionAndCB_PreamblesPerSSB_r16);
    else
      config_info = get_ssb_ro_preambles_4step(nr_rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB);
    if (twostep->msgA_TotalNumberOfRA_Preambles_r16)
      nb_of_preambles = *twostep->msgA_TotalNumberOfRA_Preambles_r16;
  }

  int groupOffset = 0;
  if (groupBconfigured) {
    AssertFatal(preamb_ga < nb_of_preambles, "Nb of preambles for groupA not compatible with total number of preambles\n");
    if (!ra->RA_GroupA) {  // groupB
      groupOffset = preamb_ga;
      nb_of_preambles = nb_of_preambles - preamb_ga;
    } else {
      nb_of_preambles = preamb_ga;
    }
  }
  int rand_preamb = (rand_r(&seed) % config_info.preambles_per_ssb);
  if (config_info.ssb_per_ro < 1)
    ra->ra_PreambleIndex = groupOffset + rand_preamb;
  else {
    ssb_list_info_t *ssb_list = &mac->ssb_list[mac->current_DL_BWP->bwp_id];
    int nb_ssb = ssb_list->nb_ssb_per_index[ra->ra_ssb];
    ra->ra_PreambleIndex = groupOffset + (nb_ssb * config_info.preambles_per_ssb) + rand_preamb;
  }
  AssertFatal(ra->ra_PreambleIndex < nb_of_preambles,
              "Error! Selected preamble %d which exceeds number of prambles available %d\n",
              ra->ra_PreambleIndex,
              nb_of_preambles);
}

// 38.321 Section 5.1.2 Random Access Resource selection
static void ra_resource_selection(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;
  ra->ra_ssb = -1; // init as not selected
  NR_RACH_ConfigDedicated_t *rach_ConfigDedicated = ra->rach_ConfigDedicated;
  if (ra->ra_type == RA_4_STEP) {
    // TODO if the Random Access procedure was initiated for SpCell beam failure recovery
    // TODO if the Random Access procedure was initiated for SI request

    if (ra->pdcch_order.active && ra->pdcch_order.preamble_index != 0xb000000) {
      // set the PREAMBLE_INDEX to the signalled ra-PreambleIndex;
      ra->ra_PreambleIndex = ra->pdcch_order.preamble_index;
      // select the SSB signalled by PDCCH
      ra->ra_ssb = ra->pdcch_order.ssb_index;
    } else if (rach_ConfigDedicated) {
      NR_CFRA_t *cfra = rach_ConfigDedicated->cfra;
      AssertFatal(cfra->occasions == NULL, "Dedicated PRACH occasions for CFRA not supported\n");
      AssertFatal(cfra->resources.present == NR_CFRA__resources_PR_ssb, "SSB-based CFRA supported only\n");
      struct NR_CFRA__resources__ssb *ssb_list = cfra->resources.choice.ssb;
      for (int i = 0; i < ssb_list->ssb_ResourceList.list.count; i++) {
        NR_CFRA_SSB_Resource_t *res = ssb_list->ssb_ResourceList.list.array[i];
        // TODO select an SSB with SS-RSRP above rsrp-ThresholdSSB amongst the associated SSBs
        if (res->ssb == mac->mib_ssb) {
          ra->ra_ssb = mac->mib_ssb;
          // set the PREAMBLE_INDEX to a ra-PreambleIndex corresponding to the selected SSB
          ra->ra_PreambleIndex = res->ra_PreambleIndex;
          break;
        }
      }
    } else {  // for the contention-based Random Access preamble selection
      // TODO if at least one of the SSBs with SS-RSRP above rsrp-ThresholdSSB is available
      // else select any SSB
      ra->ra_ssb = mac->mib_ssb;
      config_preamble_index(mac);
    }
  } else { // 2-step RA
    // if the contention-free 2-step RA type Resources associated with SSBs have been explicitly provided in rach-ConfigDedicated
    // TODO and at least one SSB with SS-RSRP above msgA-RSRP-ThresholdSSB amongst the associated SSBs is available
    if (ra->cfra) {
      AssertFatal(rach_ConfigDedicated->ext1 && rach_ConfigDedicated->ext1->cfra_TwoStep_r16,
                  "Two-step CFRA should be configured here\n");
      NR_CFRA_TwoStep_r16_t *cfra = rach_ConfigDedicated->ext1->cfra_TwoStep_r16;
      AssertFatal(cfra->occasionsTwoStepRA_r16 == NULL, "Dedicated PRACH occasions for CFRA not supported\n");
      for (int i = 0; i < cfra->resourcesTwoStep_r16.ssb_ResourceList.list.count; i++) {
        NR_CFRA_SSB_Resource_t *res = cfra->resourcesTwoStep_r16.ssb_ResourceList.list.array[i];
        if (res->ssb == mac->mib_ssb) {
          ra->ra_ssb = mac->mib_ssb;
          // set the PREAMBLE_INDEX to a ra-PreambleIndex corresponding to the selected SSB
          ra->ra_PreambleIndex = res->ra_PreambleIndex;
          break;
        }
      }
    } else { // for the contention-based Random Access Preamble selection
      // TODO if at least one of the SSBs with SS-RSRP above msgA-RSRP-ThresholdSSB is available
      // else select any SSB
      ra->ra_ssb = mac->mib_ssb;
      config_preamble_index(mac);
    }
  }
}

// Random Access procedure initialization as per 5.1.1 and initialization of variables specific
// to Random Access type as specified in clause 5.1.1a (3GPP TS 38.321 version 16.2.1 Release 16)
static bool init_RA(NR_UE_MAC_INST_t *mac, int frame)
{
  RA_config_t *ra = &mac->ra;
  // Delay init RA procedure to allow the convergence of the IIR filter on PRACH noise measurements at gNB side
  LOG_D(NR_MAC,
        "ra->ra_state %d frame %d mac->first_sync_frame %d xxx %d",
        ra->ra_state,
        frame,
        mac->first_sync_frame,
        ((MAX_FRAME_NUMBER + frame - mac->first_sync_frame) % MAX_FRAME_NUMBER) > 10);
  if ((mac->first_sync_frame > -1 || get_softmodem_params()->do_ra || get_softmodem_params()->nsa)
      && ((MAX_FRAME_NUMBER + frame - mac->first_sync_frame) % MAX_FRAME_NUMBER) > 150) {
    ra->ra_state = nrRA_GENERATE_PREAMBLE;
    LOG_D(NR_MAC, "PRACH Condition met: ra state %d, frame %d, sync_frame %d\n", ra->ra_state, frame, mac->first_sync_frame);
  } else {
    LOG_D(NR_MAC, "PRACH Condition not met: ra state %d, frame %d, sync_frame %d\n", ra->ra_state, frame, mac->first_sync_frame);
    return false;
  }

  // TODO this piece of code is required to compute MSG3_size that is used by ra_preambles_config function
  // Not a good implementation, it needs improvements
  int size_sdu = 0;

  // Concerning the C-RNTI MAC CE, it has to be included if the UL transmission (Msg3) is not being made for the CCCH logical channel.
  // Therefore it has been assumed that this event only occurs only when RA is done and it is not SA mode.
  if (get_softmodem_params()->nsa) {

    uint8_t mac_sdus[34*1056];
    uint16_t sdu_lengths[NB_RB_MAX] = {0};
    int TBS_bytes = 848;
    int mac_ce_len = 0;
    unsigned short post_padding = 1;

    // fill ulsch_buffer with random data
    for (int i = 0; i < TBS_bytes; i++){
      mac_sdus[i] = (unsigned char) (rand()&0xff);
    }
    //Sending SDUs with size 1
    //Initialize elements of sdu_lengths
    sdu_lengths[0] = TBS_bytes - 3 - post_padding - mac_ce_len;
    size_sdu += sdu_lengths[0];

    if (size_sdu > 0) {
      memcpy(ra->cont_res_id, mac_sdus, sizeof(uint8_t) * 6);
      ra->Msg3_size = size_sdu + sizeof(NR_MAC_SUBHEADER_SHORT) + sizeof(NR_MAC_SUBHEADER_SHORT);
    }

  } else if (!get_softmodem_params()->sa)
    ra->Msg3_size = sizeof(uint16_t) + sizeof(NR_MAC_SUBHEADER_FIXED);

  // Random acces procedure initialization
  mac->state = UE_PERFORMING_RA;
  ra->RA_active = true;
  NR_PRACH_RESOURCES_t *prach_resources = &ra->prach_resources;
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;
  // flush MSG3 buffer
  free_and_zero(ra->Msg3_buffer);
  // set the PREAMBLE_TRANSMISSION_COUNTER to 1
  prach_resources->preamble_tx_counter = 1;
  // set the PREAMBLE_POWER_RAMPING_COUNTER to 1
  prach_resources->preamble_power_ramping_cnt = 1;
  // set the PREAMBLE_BACKOFF to 0 ms TODO to be set as a timer?
  prach_resources->preamble_backoff = 0;
  // set POWER_OFFSET_2STEP_RA to 0 dB
  prach_resources->power_offset_2step = 0;

  // perform the BWP operation as specified in clause 5.15
  // if PRACH occasions are not configured for the active UL BWP
  if (!mac->current_UL_BWP->rach_ConfigCommon) {
    // switch the active UL BWP to BWP indicated by initialUplinkBWP
    mac->current_UL_BWP = get_ul_bwp_structure(mac, 0, false);
    // if the Serving Cell is an SpCell
    // switch the active DL BWP to BWP indicated by initialDownlinkBWP
    mac->current_DL_BWP = get_dl_bwp_structure(mac, 0, false);
  } else {
    // if the active DL BWP does not have the same bwp-Id as the active UL BWP
    if (mac->current_UL_BWP->bwp_id != mac->current_DL_BWP->bwp_id) {
      // switch the active DL BWP to the DL BWP with the same bwp-Id as the active UL BWP
      mac->current_DL_BWP = get_dl_bwp_structure(mac, 0, false);
    }
  }
  const NR_UE_UL_BWP_t *current_UL_BWP = mac->current_UL_BWP;
  NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = current_UL_BWP->rach_ConfigCommon;
  AssertFatal(nr_rach_ConfigCommon, "rach-ConfigCommon should be configured here\n");
  // stop the bwp-InactivityTimer associated with the active DL BWP of this Serving Cell, if running
  // TODO bwp-InactivityTimer not implemented

  // if the carrier to use for the Random Access procedure is explicitly signalled (always the case for us)
  // PRACH shall be as specified for QPSK modulated DFT-s-OFDM of equivalent RB allocation (38.101-1)
  AssertFatal(nr_rach_ConfigCommon->msg1_SubcarrierSpacing, "Cannot handle the scenario without msg1-SubcarrierSpacing (L839)\n");
  NR_SubcarrierSpacing_t prach_scs = *nr_rach_ConfigCommon->msg1_SubcarrierSpacing;
  int n_prbs = get_N_RA_RB(prach_scs, current_UL_BWP->scs);
  NR_RACH_ConfigGeneric_t *rach_ConfigGeneric = &nr_rach_ConfigCommon->rach_ConfigGeneric;
  int start_prb = rach_ConfigGeneric->msg1_FrequencyStart + current_UL_BWP->BWPStart;
  prach_resources->Pc_max = nr_get_Pcmax(mac->p_Max,
                                         mac->nr_band,
                                         mac->frame_type,
                                         mac->frequency_range,
                                         current_UL_BWP->channel_bandwidth,
                                         2,
                                         false,
                                         prach_scs,
                                         cfg->carrier_config.dl_grid_size[prach_scs],
                                         true,
                                         n_prbs,
                                         start_prb);


  // TODO if the Random Access procedure was initiated for SI request
  // and the Random Access Resources for SI request have been explicitly provided by RRC

  // TODO if the Random Access procedure was initiated for SpCell beam failure recovery
  // and if the contention-free Random Access Resources for beam failure recovery request for 4-step RA type
  // have been explicitly provided by RRC for the BWP selected for Random Access procedure

  NR_RACH_ConfigCommonTwoStepRA_r16_t *twostep_conf = NULL;
  NR_RACH_ConfigDedicated_t *rach_Dedicated = ra->rach_ConfigDedicated;

  // if the Random Access procedure is initiated by PDCCH order
  // and if the ra-PreambleIndex explicitly provided by PDCCH is not 0b000000
  bool pdcch_order = (ra->pdcch_order.active && ra->pdcch_order.preamble_index != 0xb000000) ? true : false;
  // or if the Random Access procedure was initiated for reconfiguration with sync and
  // if the contention-free Random Access Resources for 4-step RA type have been explicitly provided
  // in rach-ConfigDedicated for the BWP selected for Random Access procedure
  if ((rach_Dedicated && rach_Dedicated->cfra) || pdcch_order) {
    LOG_I(MAC, "Initialization of 4-Step CFRA procedure\n");
    ra->ra_type = RA_4_STEP;
    ra->cfra = true;
  } else {
    bool twostep_cfra = (rach_Dedicated && rach_Dedicated->ext1) ? (rach_Dedicated->ext1->cfra_TwoStep_r16 ? true : false) : false;
    if (twostep_cfra) {
      // if the Random Access procedure was initiated for reconfiguration with sync and
      // if the contention-free Random Access Resources for 2-step RA type have been explicitly provided in rach-ConfigDedicated
      LOG_I(MAC, "Initialization of 2-Step CFRA procedure\n");
      ra->ra_type = RA_2_STEP;
      ra->cfra = true;
    } else {
      bool twostep = false;
      if (current_UL_BWP->msgA_ConfigCommon_r16) {
        twostep_conf = &current_UL_BWP->msgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16;
        if (nr_rach_ConfigCommon) {
          // if the BWP selected for Random Access procedure is configured with both 2- and 4-step RA type Random Access Resources
          // and the RSRP of the downlink pathloss reference is above msgA-RSRP-Threshold
          AssertFatal(twostep_conf->msgA_RSRP_Threshold_r16,
                      "msgA_RSRP_Threshold_r16 is mandatory present if both 2-step and 4-step random access types are configured\n");
          // For thresholds the RSRP value is (IE value – 156) dBm except for the IE 127 in which case the actual value is infinity
          int rsrp_msga_thr = *twostep_conf->msgA_RSRP_Threshold_r16 - 156;
          if (*twostep_conf->msgA_RSRP_Threshold_r16 != 127 && mac->ssb_measurements.ssb_rsrp_dBm > rsrp_msga_thr)
            twostep = true;
        } else {
          // if the BWP selected for Random Access procedure is only configured with 2-step RA type Random Access
          twostep = true;
        }
      }
      if (twostep) {
        LOG_I(MAC, "Initialization of 2-Step CBRA procedure\n");
        ra->ra_type = RA_2_STEP;
        ra->cfra = false;
      } else {
        LOG_I(MAC, "Initialization of 4-Step CBRA procedure\n");
        ra->ra_type = RA_4_STEP;
        ra->cfra = false;
      }
    }
  }

  NR_RACH_ConfigGenericTwoStepRA_r16_t *twostep_generic = twostep_conf ? &twostep_conf->rach_ConfigGenericTwoStepRA_r16 : NULL;
  if (ra->ra_type == RA_2_STEP && twostep_generic && twostep_generic->msgA_PreamblePowerRampingStep_r16)
    prach_resources->preamble_power_ramping_step = *twostep_generic->msgA_PreamblePowerRampingStep_r16 << 1;
  else
    prach_resources->preamble_power_ramping_step = rach_ConfigGeneric->powerRampingStep << 1;

  prach_resources->scaling_factor_bi = 1;

  if (ra->ra_type == RA_2_STEP && twostep_generic && twostep_generic->preambleTransMax_r16)
    set_preambleTransMax(ra, *twostep_generic->preambleTransMax_r16);
  else
    set_preambleTransMax(ra, rach_ConfigGeneric->preambleTransMax);

  // TODO msgA-TransMax not implemented
  // TODO ra-Prioritization not implemented

  if (ra->ra_type == RA_2_STEP && twostep_generic && twostep_generic->msgA_PreambleReceivedTargetPower_r16)
    ra->preambleRxTargetPower = *twostep_generic->msgA_PreambleReceivedTargetPower_r16;
  else
    ra->preambleRxTargetPower = rach_ConfigGeneric->preambleReceivedTargetPower;

  if (ra->ra_type == RA_2_STEP
      && current_UL_BWP->msgA_ConfigCommon_r16
      && current_UL_BWP->msgA_ConfigCommon_r16->msgA_PUSCH_Config_r16
      && current_UL_BWP->msgA_ConfigCommon_r16->msgA_PUSCH_Config_r16->msgA_DeltaPreamble_r16)
    ra->deltaPreamble = *current_UL_BWP->msgA_ConfigCommon_r16->msgA_PUSCH_Config_r16->msgA_DeltaPreamble_r16 << 1;
  else
    ra->deltaPreamble = mac->current_UL_BWP->msg3_DeltaPreamble ? *mac->current_UL_BWP->msg3_DeltaPreamble << 1 : 0;

  ra->RA_RAPID_found = 0;
  ra->RA_backoff_cnt = 0;
  ra->RA_window_cnt = -1;

  // perform the Random Access Resource selection procedure (see clause 5.1.2 and .2a)
  ra_resource_selection(mac);
  return true;
}

/* TS 38.321 subclause 7.3 - return DELTA_PREAMBLE values in dB */
static int8_t nr_get_DELTA_PREAMBLE(NR_UE_MAC_INST_t *mac, uint16_t prach_format)
{
  NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = mac->current_UL_BWP->rach_ConfigCommon;
  NR_SubcarrierSpacing_t scs = *nr_rach_ConfigCommon->msg1_SubcarrierSpacing;
  int prach_sequence_length = nr_rach_ConfigCommon->prach_RootSequenceIndex.present - 1;
  uint8_t prachConfigIndex, mu;

  // SCS configuration from msg1_SubcarrierSpacing and table 4.2-1 in TS 38.211

  switch (scs){
    case NR_SubcarrierSpacing_kHz15:
    mu = 0;
    break;

    case NR_SubcarrierSpacing_kHz30:
    mu = 1;
    break;

    case NR_SubcarrierSpacing_kHz60:
    mu = 2;
    break;

    case NR_SubcarrierSpacing_kHz120:
    mu = 3;
    break;

    case NR_SubcarrierSpacing_kHz240:
    mu = 4;
    break;

    case NR_SubcarrierSpacing_kHz480_v1700:
    mu = 5;
    break;

    case NR_SubcarrierSpacing_kHz960_v1700:
    mu = 6;
    break;

    case NR_SubcarrierSpacing_spare1:
    mu = 7;
    break;

    default:
    AssertFatal(1 == 0,"Unknown msg1_SubcarrierSpacing %lu\n", scs);
  }

  // Preamble formats given by prach_ConfigurationIndex and tables 6.3.3.2-2 and 6.3.3.2-2 in TS 38.211

  prachConfigIndex = nr_rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;

  if (prach_sequence_length == 0) {
    AssertFatal(prach_format < 4, "Illegal PRACH format %d for sequence length 839\n", prach_format);
    switch (prach_format) {

      // long preamble formats
      case 0:
      case 3:
      return  0;

      case 1:
      return -3;

      case 2:
      return -6;
    }
  } else {
    switch (prach_format) { // short preamble formats
      case 0:
      case 3:
      return 8 + 3*mu;

      case 1:
      case 4:
      case 8:
      return 5 + 3*mu;

      case 2:
      case 5:
      return 3 + 3*mu;

      case 6:
      return 3*mu;

      case 7:
      return 5 + 3*mu;

      default:
      AssertFatal(1 == 0, "[UE %d] ue_procedures.c: FATAL, Illegal preambleFormat %d, prachConfigIndex %d\n",
                  mac->ue_id,
                  prach_format,
                  prachConfigIndex);
    }
  }
  return 0;
}

// TS 38.321 subclause 5.1.3 - RA preamble transmission - ra_PREAMBLE_RECEIVED_TARGET_POWER configuration
// Measurement units:
// - preambleReceivedTargetPower      dBm (-202..-60, 2 dBm granularity)
// - delta_preamble                   dB
// - RA_PREAMBLE_POWER_RAMPING_STEP   dB
// - POWER_OFFSET_2STEP_RA            dB
// returns receivedTargerPower in dBm
static int nr_get_Po_NOMINAL_PUSCH(NR_UE_MAC_INST_t *mac, NR_PRACH_RESOURCES_t *prach_resources)
{
  int8_t receivedTargerPower;
  int8_t delta_preamble;

  NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = mac->current_UL_BWP->rach_ConfigCommon;
  long preambleReceivedTargetPower = nr_rach_ConfigCommon->rach_ConfigGeneric.preambleReceivedTargetPower;
  delta_preamble = nr_get_DELTA_PREAMBLE(mac, prach_resources->prach_format);

  receivedTargerPower = preambleReceivedTargetPower +
                        delta_preamble +
                        (prach_resources->preamble_power_ramping_cnt - 1) * prach_resources->preamble_power_ramping_step +
                        prach_resources->power_offset_2step;

  LOG_D(MAC, "ReceivedTargerPower is %d dBm \n", receivedTargerPower);
  return receivedTargerPower;
}

// This routine implements Section 5.1.2 (UE Random Access Resource Selection)
// and Section 5.1.3 (Random Access Preamble Transmission) from 3GPP TS 38.321
// - currently the PRACH preamble is set through RRC configuration for 4-step CFRA mode
// todo:
// - determine next available PRACH occasion
// -- if RA initiated for SI request and ra_AssociationPeriodIndex and si-RequestPeriod are configured
// -- else if SSB is selected above
// -- else if CSI-RS is selected above
// - switch initialisation cases
// -- RA initiated by beam failure recovery operation (subclause 5.17 TS 38.321)
// --- SSB selection, set ra_PreambleIndex
// -- RA initiated by PDCCH: ra_preamble_index provided by PDCCH && ra_PreambleIndex != 0b000000
// --- set PREAMBLE_INDEX to ra_preamble_index
// --- select the SSB signalled by PDCCH
// -- RA initiated for SI request:
// --- SSB selection, set ra_PreambleIndex
// - condition on notification of suspending power ramping counter from lower layer (5.1.3 TS 38.321)
// - check if SSB or CSI-RS have not changed since the selection in the last RA Preamble tranmission
// - Contention-based RA preamble selection:
// -- selection of SSB with SS-RSRP above rsrp-ThresholdSSB else select any SSB
static void nr_get_prach_resources(NR_UE_MAC_INST_t *mac,
                                   NR_PRACH_RESOURCES_t *prach_resources,
                                   NR_RACH_ConfigDedicated_t *rach_ConfigDedicated)
{
  if (prach_resources->preamble_tx_counter > 1)
    prach_resources->preamble_power_ramping_cnt++;
  prach_resources->ra_PREAMBLE_RECEIVED_TARGET_POWER = nr_get_Po_NOMINAL_PUSCH(mac, prach_resources);
}

// TbD: RA_attempt_number not used
void nr_Msg1_transmitted(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;
  ra->ra_state = nrRA_WAIT_RAR;
  ra->RA_attempt_number++;
}

void nr_Msg3_transmitted(NR_UE_MAC_INST_t *mac, uint8_t CC_id, frame_t frameP, slot_t slotP, uint8_t gNB_id)
{
  RA_config_t *ra = &mac->ra;
  NR_RACH_ConfigCommon_t *nr_rach_ConfigCommon = mac->current_UL_BWP->rach_ConfigCommon;
  const double ta_Common_ms = get_ta_Common_ms(mac->sc_info.ntn_Config_r17);
  const int mu = mac->current_UL_BWP->scs;
  const int slots_per_ms = nr_slots_per_frame[mu] / 10;

  // start contention resolution timer
  const int RA_contention_resolution_timer_ms = (nr_rach_ConfigCommon->ra_ContentionResolutionTimer + 1) << 3;
  const int RA_contention_resolution_timer_slots = RA_contention_resolution_timer_ms * slots_per_ms;
  const int ta_Common_slots = (int)ceil(ta_Common_ms * slots_per_ms);

  // timer step 1 slot and timer target given by ra_ContentionResolutionTimer + ta-Common-r17
  nr_timer_setup(&ra->contention_resolution_timer, RA_contention_resolution_timer_slots + ta_Common_slots, 1);
  nr_timer_start(&ra->contention_resolution_timer);

  ra->ra_state = nrRA_WAIT_CONTENTION_RESOLUTION;
}

static uint8_t *fill_msg3_crnti_pdu(RA_config_t *ra, uint8_t *pdu, uint16_t crnti)
{
  // RA triggered by UE MAC with C-RNTI in MAC CE
  LOG_D(NR_MAC, "Generating MAC CE with C-RNTI for MSG3 %x\n", crnti);
  *(NR_MAC_SUBHEADER_FIXED *)pdu = (NR_MAC_SUBHEADER_FIXED){.LCID = UL_SCH_LCID_C_RNTI, .R = 0};
  pdu += sizeof(NR_MAC_SUBHEADER_FIXED);

  // C-RNTI MAC CE (2 octets)
  uint16_t rnti_pdu = ((crnti & 0xFF) << 8) | ((crnti >> 8) & 0xFF);
  memcpy(pdu, &rnti_pdu, sizeof(rnti_pdu));
  pdu += sizeof(rnti_pdu);
  ra->t_crnti = crnti;
  return pdu;
}

static uint8_t *fill_msg3_pdu_from_rlc(NR_UE_MAC_INST_t *mac, uint8_t *pdu, int TBS_max)
{
  RA_config_t *ra = &mac->ra;
  // regular Msg3/MsgA_PUSCH with PDU coming from higher layers
  *(NR_MAC_SUBHEADER_FIXED *)pdu = (NR_MAC_SUBHEADER_FIXED){.LCID = UL_SCH_LCID_CCCH};
  pdu += sizeof(NR_MAC_SUBHEADER_FIXED);
  tbs_size_t len = mac_rlc_data_req(mac->ue_id,
                                    mac->ue_id,
                                    0,
                                    0,
                                    ENB_FLAG_NO,
                                    MBMS_FLAG_NO,
                                    0, // SRB0 for messages sent in MSG3
                                    TBS_max - sizeof(NR_MAC_SUBHEADER_FIXED), /* size of mac_ce above */
                                    (char *)pdu,
                                    0,
                                    0);
  AssertFatal(len > 0, "no data for Msg3/MsgA_PUSCH\n");
  // UE Contention Resolution Identity
  // Store the first 48 bits belonging to the uplink CCCH SDU within Msg3 to determine whether or not the
  // Random Access Procedure has been successful after reception of Msg4
  // We copy from persisted memory to another persisted memory
  memcpy(ra->cont_res_id, pdu, sizeof(uint8_t) * 6);
  pdu += len;
  return pdu;
}

void nr_get_Msg3_MsgA_PUSCH_payload(NR_UE_MAC_INST_t *mac, uint8_t *buf, int TBS_max)
{
  RA_config_t *ra = &mac->ra;

  // we already stored MSG3 in the buffer, we can use that
  if (ra->Msg3_buffer) {
    memcpy(buf, ra->Msg3_buffer, sizeof(uint8_t) * TBS_max);
    return;
  }

  uint8_t *pdu = buf;
  if (ra->msg3_C_RNTI)
    pdu = fill_msg3_crnti_pdu(ra, pdu, mac->crnti);
  else
    pdu = fill_msg3_pdu_from_rlc(mac, pdu, TBS_max);

  AssertFatal(TBS_max >= pdu - buf, "Allocated resources are not enough for Msg3/MsgA_PUSCH!\n");
  // Padding: fill remainder with 0
  LOG_D(NR_MAC, "Remaining %ld bytes, filling with padding\n", pdu - buf);
  while (pdu < buf + TBS_max - sizeof(NR_MAC_SUBHEADER_FIXED)) {
    *(NR_MAC_SUBHEADER_FIXED *)pdu = (NR_MAC_SUBHEADER_FIXED){.LCID = UL_SCH_LCID_PADDING};
    pdu += sizeof(NR_MAC_SUBHEADER_FIXED);
  }
  ra->Msg3_buffer = calloc(TBS_max, sizeof(uint8_t));
  memcpy(ra->Msg3_buffer, buf, sizeof(uint8_t) * TBS_max);
}

/**
 * Function:            handles Random Access Preamble Initialization (5.1.1 TS 38.321)
 *                      handles Random Access Response reception (5.1.4 TS 38.321)
 * Note:                In SA mode the Msg3 contains a CCCH SDU, therefore no C-RNTI MAC CE is transmitted.
 *
 * @prach_resources     pointer to PRACH resources
 * @prach_pdu           pointer to FAPI UL PRACH PDU
 * @mod_id              module ID
 * @CC_id               CC ID
 * @frame               current UL TX frame
 * @gNB_id              gNB ID
 * @nr_slot_tx          current UL TX slot
 */
void nr_ue_manage_ra_procedure(NR_UE_MAC_INST_t *mac, int CC_id, frame_t frame, uint8_t gNB_id, int nr_slot_tx)
{
  RA_config_t *ra = &mac->ra;
  NR_RACH_ConfigDedicated_t *rach_ConfigDedicated = ra->rach_ConfigDedicated;
  NR_PRACH_RESOURCES_t *prach_resources = &ra->prach_resources;
  if (ra->ra_state == nrRA_UE_IDLE) {
    bool init_success = init_RA(mac, frame);
    if (!init_success)
      return;
  }

  LOG_D(NR_MAC, "[UE %d][%d.%d]: ra_state %d, RA_active %d\n", mac->ue_id, frame, nr_slot_tx, ra->ra_state, ra->RA_active);

  if (ra->ra_state < nrRA_SUCCEEDED) {
    if (ra->RA_window_cnt != -1) { // RACH is active

      LOG_D(MAC, "[%d.%d] RA is active: RA window count %d, RA backoff count %d\n", frame, nr_slot_tx, ra->RA_window_cnt, ra->RA_backoff_cnt);

      if (ra->RA_BI_found){
        prach_resources->preamble_backoff = prach_resources->scaling_factor_bi * ra->RA_backoff_indicator;
      } else {
        prach_resources->preamble_backoff = 0;
      }

      if (ra->RA_window_cnt >= 0 && ra->RA_RAPID_found == 1) {

        if(ra->cfra) {
          // Reset RA_active flag: it disables Msg3 retransmission (8.3 of TS 38.213)
          nr_ra_succeeded(mac, gNB_id, frame, nr_slot_tx);
        }

      } else if (ra->RA_window_cnt == 0 && !ra->RA_RAPID_found && ra->ra_state != nrRA_WAIT_MSGB) {
        LOG_W(MAC, "[UE %d][%d:%d] RAR reception failed \n", mac->ue_id, frame, nr_slot_tx);

        nr_ra_failed(mac, CC_id, prach_resources, frame, nr_slot_tx);

      } else if (ra->RA_window_cnt > 0) {

        LOG_D(MAC, "[UE %d][%d.%d]: RAR not received yet (RA window count %d) \n", mac->ue_id, frame, nr_slot_tx, ra->RA_window_cnt);

        // Fill in preamble and PRACH resources
        ra->RA_window_cnt--;
        if (ra->ra_state == nrRA_GENERATE_PREAMBLE) {
          nr_get_prach_resources(mac, prach_resources, rach_ConfigDedicated);
        }
      } else if (ra->RA_backoff_cnt > 0) {

        LOG_D(MAC, "[UE %d][%d.%d]: RAR not received yet (RA backoff count %d) \n", mac->ue_id, frame, nr_slot_tx, ra->RA_backoff_cnt);

        ra->RA_backoff_cnt--;

        if ((ra->RA_backoff_cnt > 0 && ra->ra_state == nrRA_GENERATE_PREAMBLE) || ra->RA_backoff_cnt == 0) {
          nr_get_prach_resources(mac, prach_resources, rach_ConfigDedicated);
        }
      }
    }
  }

  if (nr_timer_is_active(&ra->contention_resolution_timer)) {
    nr_ue_contention_resolution(mac, CC_id, frame, nr_slot_tx, prach_resources);
  }
}

int16_t nr_get_RA_window_2Step(const NR_MsgA_ConfigCommon_r16_t *msgA_ConfigCommon_r16)
{
  int16_t ra_ResponseWindow = *msgA_ConfigCommon_r16->rach_ConfigCommonTwoStepRA_r16
                              .rach_ConfigGenericTwoStepRA_r16.msgB_ResponseWindow_r16;

  switch (ra_ResponseWindow) {
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl1:
      return 1;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl2:
      return 2;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl4:
      return 4;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl8:
      return 8;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl10:
      return 10;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl20:
      return 20;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl40:
      return 40;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl80:
      return 80;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl160:
      return 160;
      break;
    case NR_RACH_ConfigGenericTwoStepRA_r16__msgB_ResponseWindow_r16_sl320:
      return 360;
      break;
    default:
      AssertFatal(false, "illegal msgB_responseWindow value %d\n", ra_ResponseWindow);
      break;
  }
  return 0;
}

int16_t nr_get_RA_window_4Step(const NR_RACH_ConfigCommon_t *rach_ConfigCommon)
{
  int16_t ra_ResponseWindow = rach_ConfigCommon->rach_ConfigGeneric.ra_ResponseWindow;

  switch (ra_ResponseWindow) {
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl1:
      return 1;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl2:
      return 2;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl4:
      return 4;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl8:
      return 8;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl10:
      return 10;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl20:
      return 20;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl40:
      return 40;
      break;
    case NR_RACH_ConfigGeneric__ra_ResponseWindow_sl80:
      return 80;
      break;
    default:
      AssertFatal(false, "illegal ra_ResponseWindow value %d\n", ra_ResponseWindow);
      break;
  }
  return 0;
}

void nr_get_RA_window(NR_UE_MAC_INST_t *mac)
{
  RA_config_t *ra = &mac->ra;

  NR_RACH_ConfigCommon_t *setup = mac->current_UL_BWP->rach_ConfigCommon;
  AssertFatal(&setup->rach_ConfigGeneric != NULL, "In %s: FATAL! rach_ConfigGeneric is NULL...\n", __FUNCTION__);
  const double ta_Common_ms = get_ta_Common_ms(mac->sc_info.ntn_Config_r17);
  const int mu = mac->current_DL_BWP->scs;
  const int slots_per_ms = nr_slots_per_frame[mu] / 10;

  const int ra_Offset_slots = ra->RA_offset * nr_slots_per_frame[mu];
  const int ta_Common_slots = (int)ceil(ta_Common_ms * slots_per_ms);

  ra->RA_window_cnt = ra_Offset_slots + ta_Common_slots; // taking into account the 2 frames gap introduced by OAI gNB

  ra->RA_window_cnt += ra->ra_type == RA_2_STEP ? nr_get_RA_window_2Step(mac->current_UL_BWP->msgA_ConfigCommon_r16)
                                                : nr_get_RA_window_4Step(mac->current_UL_BWP->rach_ConfigCommon);
}

////////////////////////////////////////////////////////////////////////////
/////////* Random Access Contention Resolution (5.1.35 TS 38.321) */////////
////////////////////////////////////////////////////////////////////////////
// Handling contention resolution timer
// WIP todo:
// - beam failure recovery
// - RA completed
void nr_ue_contention_resolution(NR_UE_MAC_INST_t *mac, int cc_id, frame_t frame, int slot, NR_PRACH_RESOURCES_t *prach_resources)
{
  RA_config_t *ra = &mac->ra;

  if (nr_timer_expired(&ra->contention_resolution_timer)) {
    ra->t_crnti = 0;
    nr_timer_stop(&ra->contention_resolution_timer);
    // Signal PHY to quit RA procedure
    LOG_E(MAC, "[UE %d] 4-Step CBRA: Contention resolution timer has expired, RA procedure has failed...\n", mac->ue_id);
    nr_ra_failed(mac, cc_id, prach_resources, frame, slot);
  }
}

// Handlig successful RA completion @ MAC layer
// according to section 5 of 3GPP TS 38.321 version 16.2.1 Release 16
// todo:
// - complete handling of received contention-based RA preamble
void nr_ra_succeeded(NR_UE_MAC_INST_t *mac, const uint8_t gNB_index, const frame_t frame, const int slot)
{
  RA_config_t *ra = &mac->ra;

  if (ra->cfra) {
    LOG_I(MAC, "[UE %d][%d.%d][RAPROC] RA procedure succeeded. CFRA: RAR successfully received.\n", mac->ue_id, frame, slot);
    ra->RA_window_cnt = -1;
  } else if (ra->ra_type == RA_2_STEP) {
    LOG_A(MAC,
          "[UE %d][%d.%d][RAPROC] 2-Step RA procedure succeeded. CBRA: Contention Resolution is successful.\n",
          mac->ue_id,
          frame,
          slot);
    mac->crnti = ra->t_crnti;
    ra->t_crnti = 0;
    LOG_D(MAC, "[UE %d][%d.%d] CBRA: cleared response window timer...\n", mac->ue_id, frame, slot);
  } else {
    LOG_A(MAC,
          "[UE %d][%d.%d][RAPROC] 4-Step RA procedure succeeded. CBRA: Contention Resolution is successful.\n",
          mac->ue_id,
          frame,
          slot);
    nr_timer_stop(&ra->contention_resolution_timer);
    mac->crnti = ra->t_crnti;
    ra->t_crnti = 0;
    LOG_D(MAC, "[UE %d][%d.%d] CBRA: cleared contention resolution timer...\n", mac->ue_id, frame, slot);
  }

  LOG_D(MAC, "[UE %d] clearing RA_active flag...\n", mac->ue_id);
  ra->RA_active = false;
  ra->msg3_C_RNTI = false;
  ra->ra_state = nrRA_SUCCEEDED;
  mac->state = UE_CONNECTED;
  free_and_zero(ra->Msg3_buffer);
  nr_mac_rrc_ra_ind(mac->ue_id, frame, true);
}

// Handling failure of RA procedure @ MAC layer
// according to section 5 of 3GPP TS 38.321 version 16.2.1 Release 16
// todo:
// - complete handling of received contention-based RA preamble
void nr_ra_failed(NR_UE_MAC_INST_t *mac, uint8_t CC_id, NR_PRACH_RESOURCES_t *prach_resources, frame_t frame, int slot)
{
  RA_config_t *ra = &mac->ra;
  // Random seed generation
  unsigned int seed;

  if (IS_SOFTMODEM_IQPLAYER || IS_SOFTMODEM_IQRECORDER) {
    // Overwrite seed with non-random seed for IQ player/recorder
    seed = 1;
  } else {
    // & to truncate the int64_t and keep only the LSB bits, up to sizeof(int)
    seed = (unsigned int) (rdtsc_oai() & ~0);
  }
  
  ra->ra_PreambleIndex = -1;
  ra->ra_state = nrRA_UE_IDLE;

  prach_resources->preamble_tx_counter++;

  // when the Contention Resolution is considered not successful
  // stop timeAlignmentTimer
  nr_timer_stop(&mac->time_alignment_timer);

  if (prach_resources->preamble_tx_counter == ra->preambleTransMax + 1) {

    LOG_D(NR_MAC,
          "[UE %d][%d.%d] Maximum number of RACH attempts (%d) reached, selecting backoff time...\n",
          mac->ue_id,
          frame,
          slot,
          ra->preambleTransMax);

    ra->RA_backoff_cnt = rand_r(&seed) % (prach_resources->preamble_backoff + 1);
    prach_resources->preamble_tx_counter = 1;
    prach_resources->preamble_power_ramping_step += 2; // 2 dB increment
    prach_resources->ra_PREAMBLE_RECEIVED_TARGET_POWER = nr_get_Po_NOMINAL_PUSCH(mac, prach_resources);

  } else {
    nr_get_RA_window(mac);
  }
}

void trigger_MAC_UE_RA(NR_UE_MAC_INST_t *mac, dci_pdu_rel15_t *pdcch_order)
{
  LOG_W(NR_MAC, "Triggering new RA procedure for UE with RNTI %x\n", mac->crnti);
  mac->state = UE_SYNC;
  reset_ra(mac, false);
  RA_config_t *ra = &mac->ra;
  ra->msg3_C_RNTI = true;
  if (pdcch_order) {
    ra->pdcch_order.active = true;
    ra->pdcch_order.preamble_index = pdcch_order->ra_preamble_index;
    ra->pdcch_order.ssb_index = pdcch_order->ss_pbch_index;
    ra->pdcch_order.prach_mask = pdcch_order->prach_mask_index;
  }
}

void prepare_msg4_msgb_feedback(NR_UE_MAC_INST_t *mac, int pid, int ack_nack)
{
  NR_UE_HARQ_STATUS_t *current_harq = &mac->dl_harq_info[pid];
  int sched_slot = current_harq->ul_slot;
  int sched_frame = current_harq->ul_frame;
  mac->nr_ue_emul_l1.num_harqs = 1;
  PUCCH_sched_t pucch = {.n_CCE = current_harq->n_CCE,
                         .N_CCE = current_harq->N_CCE,
                         .ack_payload = ack_nack,
                         .n_harq = 1};
  current_harq->active = false;
  current_harq->ack_received = false;
  if (get_softmodem_params()->emulate_l1) {
    mac->nr_ue_emul_l1.harq[pid].active = true;
    mac->nr_ue_emul_l1.harq[pid].active_dl_harq_sfn = sched_frame;
    mac->nr_ue_emul_l1.harq[pid].active_dl_harq_slot = sched_slot;
  }
  fapi_nr_ul_config_request_pdu_t *pdu = lockGet_ul_config(mac, sched_frame, sched_slot, FAPI_NR_UL_CONFIG_TYPE_PUCCH);
  if (!pdu)
    return;
  int ret = nr_ue_configure_pucch(mac, sched_slot, sched_frame, mac->ra.t_crnti, &pucch, &pdu->pucch_config_pdu);
  if (ret != 0)
    remove_ul_config_last_item(pdu);
  release_ul_config(pdu, false);
}
