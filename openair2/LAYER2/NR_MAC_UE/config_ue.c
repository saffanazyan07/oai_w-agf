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

/* \file config_ue.c
 * \brief UE and eNB configuration performed by RRC or as a consequence of RRC procedures
 * \author R. Knopp, K.H. HSU
 * \date 2018
 * \version 0.1
 * \company Eurecom / NTUST
 * \email: knopp@eurecom.fr, kai-hsiang.hsu@eurecom.fr
 * \note
 * \warning
 */

//#include "mac_defs.h"
#include <NR_MAC_gNB/mac_proto.h>
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC-CellGroupConfig.h"
#include "LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "common/utils/nr/nr_common.h"
#include "executables/softmodem-common.h"
#include "SCHED_NR/phy_frame_config_nr.h"

void set_tdd_config_nr_ue(fapi_nr_tdd_table_t *tdd_table,
                          int mu,
                          NR_TDD_UL_DL_Pattern_t *pattern)
{
  const int nrofDownlinkSlots = pattern->nrofDownlinkSlots;
  const int nrofDownlinkSymbols = pattern->nrofDownlinkSymbols;
  const int nrofUplinkSlots = pattern->nrofUplinkSlots;
  const int nrofUplinkSymbols = pattern->nrofUplinkSymbols;
  const int nb_periods_per_frame = get_nb_periods_per_frame(pattern->dl_UL_TransmissionPeriodicity);
  const int nb_slots_per_period = ((1 << mu) * NR_NUMBER_OF_SUBFRAMES_PER_FRAME) / nb_periods_per_frame;
  tdd_table->tdd_period_in_slots = nb_slots_per_period;

  if ((nrofDownlinkSymbols + nrofUplinkSymbols) == 0)
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  else {
    AssertFatal(nrofDownlinkSymbols + nrofUplinkSymbols < 14,"illegal symbol configuration DL %d, UL %d\n",nrofDownlinkSymbols,nrofUplinkSymbols);
    AssertFatal(nb_slots_per_period == (nrofDownlinkSlots + nrofUplinkSlots + 1),
                "set_tdd_configuration_nr: given period is inconsistent with current tdd configuration, nrofDownlinkSlots %d, nrofUplinkSlots %d, nrofMixed slots 1, nb_slots_per_period %d \n",
                nrofDownlinkSlots,nrofUplinkSlots,nb_slots_per_period);
  }

  tdd_table->max_tdd_periodicity_list = (fapi_nr_max_tdd_periodicity_t *) malloc(nb_slots_per_period * sizeof(fapi_nr_max_tdd_periodicity_t));

  for(int memory_alloc = 0 ; memory_alloc < nb_slots_per_period; memory_alloc++)
    tdd_table->max_tdd_periodicity_list[memory_alloc].max_num_of_symbol_per_slot_list =
      (fapi_nr_max_num_of_symbol_per_slot_t *) malloc(NR_NUMBER_OF_SYMBOLS_PER_SLOT*sizeof(fapi_nr_max_num_of_symbol_per_slot_t));

  int slot_number = 0;
  while(slot_number != nb_slots_per_period) {
    if(nrofDownlinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofDownlinkSlots * NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol % NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config = 0;
        if((number_of_symbol + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }

    if (nrofDownlinkSymbols != 0 || nrofUplinkSymbols != 0) {
      for(int number_of_symbol = 0; number_of_symbol < nrofDownlinkSymbols; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 0;
      }
      for(int number_of_symbol = nrofDownlinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofUplinkSymbols; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 2;
      }
      for(int number_of_symbol = NR_NUMBER_OF_SYMBOLS_PER_SLOT - nrofUplinkSymbols; number_of_symbol < NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol].slot_config = 1;
      }
      slot_number++;
    }

    if(nrofUplinkSlots != 0) {
      for (int number_of_symbol = 0; number_of_symbol < nrofUplinkSlots * NR_NUMBER_OF_SYMBOLS_PER_SLOT; number_of_symbol++) {
        tdd_table->max_tdd_periodicity_list[slot_number].max_num_of_symbol_per_slot_list[number_of_symbol%NR_NUMBER_OF_SYMBOLS_PER_SLOT].slot_config = 1;
        if((number_of_symbol + 1) % NR_NUMBER_OF_SYMBOLS_PER_SLOT == 0)
          slot_number++;
      }
    }
  }
}

void config_common_ue_sa(NR_UE_MAC_INST_t *mac,
                         NR_ServingCellConfigCommonSIB_t *scc,
		         module_id_t module_id,
		         int cc_idP)
{
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;
  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;

  LOG_D(MAC, "Entering SA UE Config Common\n");

  // carrier config
  NR_FrequencyInfoDL_SIB_t *frequencyInfoDL = &scc->downlinkConfigCommon.frequencyInfoDL;
  AssertFatal(frequencyInfoDL->frequencyBandList.list.array[0]->freqBandIndicatorNR,
              "Field mandatory present for DL in SIB1\n");
  mac->nr_band = *frequencyInfoDL->frequencyBandList.list.array[0]->freqBandIndicatorNR;
  int bw_index = get_supported_band_index(frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                          mac->nr_band,
                                          frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.dl_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

  uint64_t dl_bw_khz = (12 * frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth) *
                       (15 << frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing);
  cfg->carrier_config.dl_frequency = (downlink_frequency[cc_idP][0]/1000) - (dl_bw_khz>>1);

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.dl_grid_size[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.dl_k0[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
    }
    else {
      cfg->carrier_config.dl_grid_size[i] = 0;
      cfg->carrier_config.dl_k0[i] = 0;
    }
  }

  NR_FrequencyInfoUL_SIB_t *frequencyInfoUL = &scc->uplinkConfigCommon->frequencyInfoUL;
  mac->p_Max = frequencyInfoUL->p_Max ?
               *frequencyInfoUL->p_Max :
               INT_MIN;
  bw_index = get_supported_band_index(frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                      mac->nr_band,
                                      frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
  cfg->carrier_config.uplink_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

  if (frequencyInfoUL->absoluteFrequencyPointA == NULL)
    cfg->carrier_config.uplink_frequency = cfg->carrier_config.dl_frequency;
  else
    // TODO check if corresponds to what reported in SIB1
    cfg->carrier_config.uplink_frequency = (downlink_frequency[cc_idP][0]/1000) + uplink_frequency_offset[cc_idP][0];

  for (int i = 0; i < 5; i++) {
    if (i == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
      cfg->carrier_config.ul_grid_size[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
      cfg->carrier_config.ul_k0[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
    }
    else {
      cfg->carrier_config.ul_grid_size[i] = 0;
      cfg->carrier_config.ul_k0[i] = 0;
    }
  }

  mac->frame_type = get_frame_type(mac->nr_band, get_softmodem_params()->numerology);
  // cell config
  cfg->cell_config.phy_cell_id = mac->physCellId;
  cfg->cell_config.frame_duplex_type = mac->frame_type;

  // SSB config
  cfg->ssb_config.ss_pbch_power = scc->ss_PBCH_BlockPower;
  cfg->ssb_config.scs_common = get_softmodem_params()->numerology;

  // SSB Table config
  cfg->ssb_table.ssb_offset_point_a = frequencyInfoDL->offsetToPointA;
  cfg->ssb_table.ssb_period = scc->ssb_PeriodicityServingCell;
  cfg->ssb_table.ssb_subcarrier_offset = mac->ssb_subcarrier_offset;

  if (mac->frequency_range == FR1){
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = ((uint32_t) scc->ssb_PositionsInBurst.inOneGroup.buf[0]) << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
  }
  else{
    for (int i=0; i<8; i++){
      if ((scc->ssb_PositionsInBurst.groupPresence->buf[0]>>(7-i))&0x01)
        cfg->ssb_table.ssb_mask_list[i>>2].ssb_mask |= scc->ssb_PositionsInBurst.inOneGroup.buf[0]<<(24-8*(i%4));
    }
  }

  // TDD Table Configuration
  if (cfg->cell_config.frame_duplex_type == TDD){
    set_tdd_config_nr_ue(&cfg->tdd_table_1,
                         cfg->ssb_config.scs_common,
                         &mac->tdd_UL_DL_ConfigurationCommon->pattern1);
    if (mac->tdd_UL_DL_ConfigurationCommon->pattern2) {
      cfg->tdd_table_2 = (fapi_nr_tdd_table_t *) malloc(sizeof(fapi_nr_tdd_table_t));
      set_tdd_config_nr_ue(cfg->tdd_table_2,
                           cfg->ssb_config.scs_common,
                           mac->tdd_UL_DL_ConfigurationCommon->pattern2);
    }
  }

  // PRACH configuration

  uint8_t nb_preambles = 64;
  NR_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP.rach_ConfigCommon->choice.setup;
  if(rach_ConfigCommon->totalNumberOfRA_Preambles != NULL)
     nb_preambles = *rach_ConfigCommon->totalNumberOfRA_Preambles;

  cfg->prach_config.prach_sequence_length = rach_ConfigCommon->prach_RootSequenceIndex.present-1;

  if (rach_ConfigCommon->msg1_SubcarrierSpacing)
    cfg->prach_config.prach_sub_c_spacing = *rach_ConfigCommon->msg1_SubcarrierSpacing;
  else {
    // If absent, the UE applies the SCS as derived from the prach-ConfigurationIndex (for 839)
    int config_index = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
    const int64_t *prach_config_info_p = get_prach_config_info(mac->frequency_range, config_index, mac->frame_type);
    int format = prach_config_info_p[0];
    cfg->prach_config.prach_sub_c_spacing = format == 3 ? 5 : 4;
  }

  cfg->prach_config.restricted_set_config = rach_ConfigCommon->restrictedSetConfig;

  switch (rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM) {
    case 0 :
      cfg->prach_config.num_prach_fd_occasions = 1;
      break;
    case 1 :
      cfg->prach_config.num_prach_fd_occasions = 2;
      break;
    case 2 :
      cfg->prach_config.num_prach_fd_occasions = 4;
      break;
    case 3 :
      cfg->prach_config.num_prach_fd_occasions = 8;
      break;
    default:
      AssertFatal(1==0,"msg1 FDM identifier %ld undefined (0,1,2,3) \n", rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM);
  }

  cfg->prach_config.num_prach_fd_occasions_list = (fapi_nr_num_prach_fd_occasions_t *) malloc(cfg->prach_config.num_prach_fd_occasions*sizeof(fapi_nr_num_prach_fd_occasions_t));
  for (int i=0; i<cfg->prach_config.num_prach_fd_occasions; i++) {
    fapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &cfg->prach_config.num_prach_fd_occasions_list[i];
    prach_fd_occasion->num_prach_fd_occasions = i;
    if (cfg->prach_config.prach_sequence_length)
      prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l139;
    else
      prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l839;
    prach_fd_occasion->k1 = NRRIV2PRBOFFSET(scc->uplinkConfigCommon->initialUplinkBWP.genericParameters.locationAndBandwidth, MAX_BWP_SIZE) +
                                            rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart +
                                            (get_N_RA_RB(cfg->prach_config.prach_sub_c_spacing, frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing ) * i);
    prach_fd_occasion->prach_zero_corr_conf = rach_ConfigCommon->rach_ConfigGeneric.zeroCorrelationZoneConfig;
    prach_fd_occasion->num_root_sequences = compute_nr_root_seq(rach_ConfigCommon,
                                                                nb_preambles, mac->frame_type, mac->frequency_range);
    //prach_fd_occasion->num_unused_root_sequences = ???
  }
  cfg->prach_config.ssb_per_rach = rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present-1;

}

void config_common_ue(NR_UE_MAC_INST_t *mac,
                      NR_ServingCellConfigCommon_t *scc,
		      module_id_t module_id,
		      int cc_idP)
{
  fapi_nr_config_request_t *cfg = &mac->phy_config.config_req;

  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;
  
  // carrier config
  LOG_D(MAC, "Entering UE Config Common\n");

  AssertFatal(scc->downlinkConfigCommon,
              "Not expecting downlinkConfigCommon to be NULL here\n");

  NR_FrequencyInfoDL_t *frequencyInfoDL = scc->downlinkConfigCommon->frequencyInfoDL;
  if (frequencyInfoDL) { // NeedM for inter-freq handover
    mac->nr_band = *frequencyInfoDL->frequencyBandList.list.array[0];
    mac->frame_type = get_frame_type(mac->nr_band, get_softmodem_params()->numerology);
    mac->frequency_range = mac->nr_band < 256 ? FR1 : FR2;

    int bw_index = get_supported_band_index(frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                            mac->nr_band,
                                            frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
    cfg->carrier_config.dl_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);
    
    cfg->carrier_config.dl_frequency = from_nrarfcn(mac->nr_band,
                                                    *scc->ssbSubcarrierSpacing,
                                                    frequencyInfoDL->absoluteFrequencyPointA)/1000; // freq in kHz
    
    for (int i = 0; i < 5; i++) {
      if (i == frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
        cfg->carrier_config.dl_grid_size[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
        cfg->carrier_config.dl_k0[i] = frequencyInfoDL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      }
      else {
        cfg->carrier_config.dl_grid_size[i] = 0;
        cfg->carrier_config.dl_k0[i] = 0;
      }
    }
  }

  if (scc->uplinkConfigCommon && scc->uplinkConfigCommon->frequencyInfoUL) {
    NR_FrequencyInfoUL_t *frequencyInfoUL = scc->uplinkConfigCommon->frequencyInfoUL;
    mac->p_Max = frequencyInfoUL->p_Max ?
                 *frequencyInfoUL->p_Max :
                 INT_MIN;

    int bw_index = get_supported_band_index(frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing,
                                            *frequencyInfoUL->frequencyBandList->list.array[0],
                                            frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth);
    cfg->carrier_config.uplink_bandwidth = get_supported_bw_mhz(mac->frequency_range, bw_index);

    long *UL_pointA = NULL;
    if (frequencyInfoUL->absoluteFrequencyPointA)
      UL_pointA = frequencyInfoUL->absoluteFrequencyPointA;
    else if (frequencyInfoDL)
      UL_pointA = &frequencyInfoDL->absoluteFrequencyPointA;

    if(UL_pointA)
      cfg->carrier_config.uplink_frequency = from_nrarfcn(*frequencyInfoUL->frequencyBandList->list.array[0],
                                                          *scc->ssbSubcarrierSpacing,
                                                          *UL_pointA) / 1000; // freq in kHz

    for (int i = 0; i < 5; i++) {
      if (i == frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->subcarrierSpacing) {
        cfg->carrier_config.ul_grid_size[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->carrierBandwidth;
        cfg->carrier_config.ul_k0[i] = frequencyInfoUL->scs_SpecificCarrierList.list.array[0]->offsetToCarrier;
      }
      else {
        cfg->carrier_config.ul_grid_size[i] = 0;
        cfg->carrier_config.ul_k0[i] = 0;
      }
    }
  }

  // cell config
  cfg->cell_config.phy_cell_id = *scc->physCellId;
  cfg->cell_config.frame_duplex_type = mac->frame_type;

  // SSB config
  cfg->ssb_config.ss_pbch_power = scc->ss_PBCH_BlockPower;
  cfg->ssb_config.scs_common = *scc->ssbSubcarrierSpacing;

  // SSB Table config
  if (frequencyInfoDL && frequencyInfoDL->absoluteFrequencySSB) {
    int scs_scaling = 1<<(cfg->ssb_config.scs_common);
    if (frequencyInfoDL->absoluteFrequencyPointA < 600000)
      scs_scaling = scs_scaling*3;
    if (frequencyInfoDL->absoluteFrequencyPointA > 2016666)
      scs_scaling = scs_scaling>>2;
    uint32_t absolute_diff = (*frequencyInfoDL->absoluteFrequencySSB - frequencyInfoDL->absoluteFrequencyPointA);
    cfg->ssb_table.ssb_offset_point_a = absolute_diff/(12*scs_scaling) - 10;
    cfg->ssb_table.ssb_period = *scc->ssb_periodicityServingCell;
    // NSA -> take ssb offset from SCS
    cfg->ssb_table.ssb_subcarrier_offset = absolute_diff%(12*scs_scaling);
  }

  switch (scc->ssb_PositionsInBurst->present) {
  case 1 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = scc->ssb_PositionsInBurst->choice.shortBitmap.buf[0] << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    break;
  case 2 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = ((uint32_t) scc->ssb_PositionsInBurst->choice.mediumBitmap.buf[0]) << 24;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    break;
  case 3 :
    cfg->ssb_table.ssb_mask_list[0].ssb_mask = 0;
    cfg->ssb_table.ssb_mask_list[1].ssb_mask = 0;
    for (int i = 0; i < 4; i++) {
      cfg->ssb_table.ssb_mask_list[0].ssb_mask += (uint32_t) scc->ssb_PositionsInBurst->choice.longBitmap.buf[3 - i] << i * 8;
      cfg->ssb_table.ssb_mask_list[1].ssb_mask += (uint32_t) scc->ssb_PositionsInBurst->choice.longBitmap.buf[7 - i] << i * 8;
    }
    break;
  default:
    AssertFatal(1==0,"SSB bitmap size value %d undefined (allowed values 1,2,3) \n", scc->ssb_PositionsInBurst->present);
  }

  // TDD Table Configuration
  if (cfg->cell_config.frame_duplex_type == TDD){
    set_tdd_config_nr_ue(&cfg->tdd_table_1,
                         cfg->ssb_config.scs_common,
                         &mac->tdd_UL_DL_ConfigurationCommon->pattern1);
    if (mac->tdd_UL_DL_ConfigurationCommon->pattern2) {
      cfg->tdd_table_2 = (fapi_nr_tdd_table_t *) malloc(sizeof(fapi_nr_tdd_table_t));
      set_tdd_config_nr_ue(cfg->tdd_table_2,
                           cfg->ssb_config.scs_common,
                           mac->tdd_UL_DL_ConfigurationCommon->pattern2);
    }
  }

  // PRACH configuration
  uint8_t nb_preambles = 64;
  if (scc->uplinkConfigCommon &&
      scc->uplinkConfigCommon->initialUplinkBWP &&
      scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon) { // all NeedM

    NR_RACH_ConfigCommon_t *rach_ConfigCommon = scc->uplinkConfigCommon->initialUplinkBWP->rach_ConfigCommon->choice.setup;
    if(rach_ConfigCommon->totalNumberOfRA_Preambles != NULL)
      nb_preambles = *rach_ConfigCommon->totalNumberOfRA_Preambles;

    cfg->prach_config.prach_sequence_length = rach_ConfigCommon->prach_RootSequenceIndex.present-1;

    if (rach_ConfigCommon->msg1_SubcarrierSpacing)
      cfg->prach_config.prach_sub_c_spacing = *rach_ConfigCommon->msg1_SubcarrierSpacing;
    else {
      // If absent, the UE applies the SCS as derived from the prach-ConfigurationIndex (for 839)
      int config_index = rach_ConfigCommon->rach_ConfigGeneric.prach_ConfigurationIndex;
      const int64_t *prach_config_info_p = get_prach_config_info(mac->frequency_range, config_index, mac->frame_type);
      int format = prach_config_info_p[0];
      cfg->prach_config.prach_sub_c_spacing = format == 3 ? 5 : 4;
    }

    cfg->prach_config.restricted_set_config = rach_ConfigCommon->restrictedSetConfig;

    switch (rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM) {
      case 0 :
        cfg->prach_config.num_prach_fd_occasions = 1;
        break;
      case 1 :
        cfg->prach_config.num_prach_fd_occasions = 2;
        break;
      case 2 :
        cfg->prach_config.num_prach_fd_occasions = 4;
        break;
      case 3 :
        cfg->prach_config.num_prach_fd_occasions = 8;
        break;
      default:
        AssertFatal(1==0,"msg1 FDM identifier %ld undefined (0,1,2,3) \n", rach_ConfigCommon->rach_ConfigGeneric.msg1_FDM);
    }

    cfg->prach_config.num_prach_fd_occasions_list = (fapi_nr_num_prach_fd_occasions_t *) malloc(cfg->prach_config.num_prach_fd_occasions*sizeof(fapi_nr_num_prach_fd_occasions_t));
    for (int i = 0; i < cfg->prach_config.num_prach_fd_occasions; i++) {
      fapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &cfg->prach_config.num_prach_fd_occasions_list[i];
      prach_fd_occasion->num_prach_fd_occasions = i;
      if (cfg->prach_config.prach_sequence_length)
        prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l139;
      else
        prach_fd_occasion->prach_root_sequence_index = rach_ConfigCommon->prach_RootSequenceIndex.choice.l839;

      prach_fd_occasion->k1 = rach_ConfigCommon->rach_ConfigGeneric.msg1_FrequencyStart;
      prach_fd_occasion->prach_zero_corr_conf = rach_ConfigCommon->rach_ConfigGeneric.zeroCorrelationZoneConfig;
      prach_fd_occasion->num_root_sequences = compute_nr_root_seq(rach_ConfigCommon,
                                                                  nb_preambles,
                                                                  mac->frame_type,
                                                                  mac->frequency_range);

      cfg->prach_config.ssb_per_rach = rach_ConfigCommon->ssb_perRACH_OccasionAndCB_PreamblesPerSSB->present-1;
      //prach_fd_occasion->num_unused_root_sequences = ???
    }
  }
}


NR_SearchSpace_t *get_common_search_space(const struct NR_PDCCH_ConfigCommon__commonSearchSpaceList *commonSearchSpaceList,
                                          const NR_UE_MAC_INST_t *mac,
                                          const NR_SearchSpaceId_t ss_id)
{
  if (ss_id == 0)
    return mac->search_space_zero;

  NR_SearchSpace_t *css = NULL;
  for (int i = 0; i < commonSearchSpaceList->list.count; i++) {
    if (commonSearchSpaceList->list.array[i]->searchSpaceId == ss_id) {
      css = calloc(1, sizeof(*css));
      memcpy(css, commonSearchSpaceList->list.array[i], sizeof(NR_SearchSpace_t));
      break;
    }
  }
  AssertFatal(css, "Couldn't find CSS with Id %ld\n", ss_id);
  return css;
}

void configure_ss_coreset(NR_UE_MAC_INST_t *mac,
                          NR_PDCCH_ConfigCommon_t *pdcch_ConfigCommon,
                          const NR_PDCCH_Config_t *pdcch_Config)
{
  // configuration of search spaces
  if (pdcch_ConfigCommon) {
    mac->otherSI_SS = pdcch_ConfigCommon->searchSpaceOtherSystemInformation ?
                      get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList, mac,
                                              *pdcch_ConfigCommon->searchSpaceOtherSystemInformation) :
                      NULL;
    mac->ra_SS = pdcch_ConfigCommon->ra_SearchSpace ?
                 get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList, mac,
                                         *pdcch_ConfigCommon->ra_SearchSpace) :
                 NULL;
    mac->paging_SS = pdcch_ConfigCommon->pagingSearchSpace ?
                     get_common_search_space(pdcch_ConfigCommon->commonSearchSpaceList, mac,
                                             *pdcch_ConfigCommon->pagingSearchSpace) :
                     NULL;
  }
  if(pdcch_Config &&
     pdcch_Config->searchSpacesToAddModList) {
    int ss_configured = 0;
    struct NR_PDCCH_Config__searchSpacesToAddModList *searchSpacesToAddModList = pdcch_Config->searchSpacesToAddModList;
    for (int i = 0; i < searchSpacesToAddModList->list.count; i++) {
      AssertFatal(ss_configured < FAPI_NR_MAX_SS, "Attempting to configure %d SS but only %d per BWP are allowed",
                  ss_configured + 1, FAPI_NR_MAX_SS);
      mac->BWP_searchspaces[ss_configured] = searchSpacesToAddModList->list.array[i];
      ss_configured++;
    }
    for (int i = ss_configured; i < FAPI_NR_MAX_SS; i++)
      mac->BWP_searchspaces[i] = NULL;
  }

  // configuration of coresets
  if (pdcch_ConfigCommon)
    updateMACie(mac->commonControlResourceSet,
                pdcch_ConfigCommon->commonControlResourceSet,
                NR_ControlResourceSet_t);

  int cset_configured = 0;
  if(pdcch_Config &&
     pdcch_Config->controlResourceSetToAddModList) {
    struct NR_PDCCH_Config__controlResourceSetToAddModList *controlResourceSetToAddModList = pdcch_Config->controlResourceSetToAddModList;
    for (int i = 0; i < controlResourceSetToAddModList->list.count; i++) {
      AssertFatal(cset_configured < FAPI_NR_MAX_CORESET_PER_BWP, "Attempting to configure %d CORESET but only %d per BWP are allowed",
                  cset_configured + 1, FAPI_NR_MAX_CORESET_PER_BWP);
      // In case network reconfigures control resource set with the same ControlResourceSetId as used for commonControlResourceSet
      // configured via PDCCH-ConfigCommon, the configuration from PDCCH-Config always takes precedence
      if (mac->commonControlResourceSet &&
          controlResourceSetToAddModList->list.array[i]->controlResourceSetId == mac->commonControlResourceSet->controlResourceSetId) {
        ASN_STRUCT_FREE(asn_DEF_NR_ControlResourceSet, mac->commonControlResourceSet);
        mac->commonControlResourceSet = NULL;
      }
      mac->BWP_coresets[cset_configured] = controlResourceSetToAddModList->list.array[i];
      cset_configured++;
    }
  }
  for (int i = cset_configured; i < FAPI_NR_MAX_CORESET_PER_BWP; i++)
    mac->BWP_coresets[i] = NULL;
}

void nr_rrc_mac_config_req_ue_logicalChannelBearer(module_id_t module_id,
                                                   struct NR_CellGroupConfig__rlc_BearerToAddModList *rlc_toadd_list,
                                                   struct NR_CellGroupConfig__rlc_BearerToReleaseList *rlc_torelease_list)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  if (rlc_toadd_list) {
    for (int i = 0; i < rlc_toadd_list->list.count; i++) {
      NR_RLC_BearerConfig_t *rlc_bearer = rlc_toadd_list->list.array[i];
      int id = rlc_bearer->logicalChannelIdentity - 1;
      mac->active_RLC_bearer[id] = true;
    }
  }
  if (rlc_torelease_list) {
    for (int i = 0; i < rlc_torelease_list->list.count; i++) {
      if (rlc_torelease_list->list.array[i]) {
        int id = *rlc_torelease_list->list.array[i] - 1;
        mac->active_RLC_bearer[id] = false;
      }
    }
  }
}

void configure_common_bwp(NR_UE_MAC_INST_t *mac,
                          NR_ServingCellConfigCommonSIB_t *scc_sib,
                          NR_ServingCellConfigCommon_t *scc,
                          NR_BWP_Id_t *first_dl,
                          NR_BWP_Id_t *first_ul)
{
  NR_UE_DL_BWP_t *dl_bwp = &mac->current_DL_BWP;
  NR_UE_UL_BWP_t *ul_bwp = &mac->current_UL_BWP;
  // SCC is provided when receiving reconfigurationWithSync
  // SCC_SIB is provided when receiving SIB1
  // So never at the same time
  if (scc) {
    AssertFatal(scc->downlinkConfigCommon,
                "Expecting dowlinkConfigCommon for HO and serving cell add\n");
    NR_BWP_DownlinkCommon_t *initial_dl_bwp = scc->downlinkConfigCommon->initialDownlinkBWP;
    if (initial_dl_bwp) {
      NR_BWP_t *dl_genericParameters = &initial_dl_bwp->genericParameters;
      dl_bwp->initial_BWPSize = NRRIV2BW(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
      dl_bwp->initial_BWPStart = NRRIV2PRBOFFSET(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
      // if the active BWP is not the initial no need to configure the initial
      if (!first_dl || *first_dl == 0) { // TODO
        AssertFatal(false, "Case not handled yet, possible in case of handover\n");
      }
    }
    if (scc->uplinkConfigCommon && scc->uplinkConfigCommon->initialUplinkBWP) { // Need M
      NR_BWP_UplinkCommon_t *initial_ul_bwp = scc->uplinkConfigCommon->initialUplinkBWP;
      NR_BWP_t *ul_genericParameters = &initial_ul_bwp->genericParameters;
      ul_bwp->initial_BWPSize = NRRIV2BW(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
      ul_bwp->initial_BWPStart = NRRIV2PRBOFFSET(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
      // if the active BWP is not the initial no need to configure the initial
      if (!first_ul || *first_ul == 0) { // TODO
        AssertFatal(false, "Case not handled yet, possible in case of handover\n");
      }
    }
  }
  if (scc_sib) {
    dl_bwp->bwp_id = 0;
    NR_BWP_DownlinkCommon_t *initial_dl_bwp = &scc_sib->downlinkConfigCommon.initialDownlinkBWP;
    NR_BWP_t *dl_genericParameters = &initial_dl_bwp->genericParameters;
    dl_bwp->initial_BWPSize = NRRIV2BW(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    dl_bwp->initial_BWPStart = NRRIV2PRBOFFSET(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    dl_bwp->scs = dl_genericParameters->subcarrierSpacing;
    updateMACie(dl_bwp->cyclicprefix,
                dl_genericParameters->cyclicPrefix,
                long);
    dl_bwp->BWPSize = NRRIV2BW(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    dl_bwp->BWPStart = NRRIV2PRBOFFSET(dl_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    AssertFatal(initial_dl_bwp->pdcch_ConfigCommon &&
                initial_dl_bwp->pdcch_ConfigCommon->present == NR_SetupRelease_PDCCH_ConfigCommon_PR_setup,
                "Not expecting missing PDCCH-ConfigCommon in SIB1\n");
    configure_ss_coreset(mac, initial_dl_bwp->pdcch_ConfigCommon->choice.setup, NULL);
    if(initial_dl_bwp->pdsch_ConfigCommon &&
       initial_dl_bwp->pdsch_ConfigCommon->present == NR_SetupRelease_PDSCH_ConfigCommon_PR_setup)
      updateMACie(dl_bwp->tdaList_Common,
                  initial_dl_bwp->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList,
                  NR_PDSCH_TimeDomainResourceAllocationList_t);

    ul_bwp->bwp_id = 0;
    AssertFatal(scc_sib->uplinkConfigCommon,
                "Not expecting UplinkConfigCommon missing in SIB1\n");
    NR_BWP_UplinkCommon_t *initial_ul_bwp = &scc_sib->uplinkConfigCommon->initialUplinkBWP;
    NR_BWP_t *ul_genericParameters = &initial_ul_bwp->genericParameters;
    ul_bwp->initial_BWPSize = NRRIV2BW(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    ul_bwp->initial_BWPStart = NRRIV2PRBOFFSET(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    ul_bwp->scs = ul_genericParameters->subcarrierSpacing;
    updateMACie(ul_bwp->cyclicprefix,
                ul_genericParameters->cyclicPrefix,
                long);
    ul_bwp->BWPSize = NRRIV2BW(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    ul_bwp->BWPStart = NRRIV2PRBOFFSET(ul_genericParameters->locationAndBandwidth, MAX_BWP_SIZE);
    AssertFatal(initial_ul_bwp->rach_ConfigCommon &&
                initial_ul_bwp->rach_ConfigCommon->present == NR_SetupRelease_RACH_ConfigCommon_PR_setup,
                "Not expecting missing RACH-ConfigCommon in SIB1\n");
    updateMACie(ul_bwp->rach_ConfigCommon,
                initial_ul_bwp->rach_ConfigCommon->choice.setup,
                NR_RACH_ConfigCommon_t);
    // Setup the SSB to Rach Occasions mapping according to the config
    build_ssb_to_ro_map(mac);
    if(initial_ul_bwp->pusch_ConfigCommon &&
       initial_ul_bwp->pusch_ConfigCommon->present == NR_SetupRelease_PUSCH_ConfigCommon_PR_setup) {
      updateMACie(ul_bwp->tdaList_Common,
                  initial_ul_bwp->pusch_ConfigCommon->choice.setup->pusch_TimeDomainAllocationList,
                  NR_PUSCH_TimeDomainResourceAllocationList_t);
      updateMACie(ul_bwp->msg3_DeltaPreamble,
                  initial_ul_bwp->pusch_ConfigCommon->choice.setup->msg3_DeltaPreamble,
                  long);
    }
    AssertFatal(initial_ul_bwp->pucch_ConfigCommon &&
                initial_ul_bwp->pucch_ConfigCommon->present == NR_SetupRelease_PUCCH_ConfigCommon_PR_setup,
                "Not expecting missing PUCCH-ConfigCommon in SIB1\n");
    updateMACie(ul_bwp->pucch_ConfigCommon,
                initial_ul_bwp->pucch_ConfigCommon->choice.setup,
                NR_PUCCH_ConfigCommon_t);

    dl_bwp->bw_tbslbrm = get_dlbw_tbslbrm(dl_bwp->initial_BWPSize, NULL);
    ul_bwp->bw_tbslbrm = get_ulbw_tbslbrm(ul_bwp->initial_BWPSize, NULL);
  }
}

void configure_current_BWP(NR_UE_MAC_INST_t *mac,
                           const NR_ServingCellConfig_t *spCellConfigDedicated)
{
  NR_UE_DL_BWP_t *DL_BWP = &mac->current_DL_BWP;
  NR_UE_UL_BWP_t *UL_BWP = &mac->current_UL_BWP;
  NR_BWP_t dl_genericParameters = {0};
  NR_BWP_t ul_genericParameters = {0};

  if(spCellConfigDedicated) {
    UL_BWP->supplementaryUplink = spCellConfigDedicated->supplementaryUplink;
    UL_BWP->csi_MeasConfig = spCellConfigDedicated->csi_MeasConfig ? spCellConfigDedicated->csi_MeasConfig->choice.setup : NULL;
    UL_BWP->pusch_servingcellconfig =
        spCellConfigDedicated->uplinkConfig && spCellConfigDedicated->uplinkConfig->pusch_ServingCellConfig ? spCellConfigDedicated->uplinkConfig->pusch_ServingCellConfig->choice.setup : NULL;
    DL_BWP->pdsch_servingcellconfig = spCellConfigDedicated->pdsch_ServingCellConfig ? spCellConfigDedicated->pdsch_ServingCellConfig->choice.setup : NULL;

    if (spCellConfigDedicated->firstActiveDownlinkBWP_Id)
      DL_BWP->bwp_id = *spCellConfigDedicated->firstActiveDownlinkBWP_Id;
    if (spCellConfigDedicated->uplinkConfig->firstActiveUplinkBWP_Id)
      UL_BWP->bwp_id = *spCellConfigDedicated->uplinkConfig->firstActiveUplinkBWP_Id;

    NR_BWP_Downlink_t *bwp_downlink = NULL;
    const struct NR_ServingCellConfig__downlinkBWP_ToAddModList *bwpList = spCellConfigDedicated->downlinkBWP_ToAddModList;
    if (bwpList)
      DL_BWP->n_dl_bwp = bwpList->list.count;
    if (bwpList && DL_BWP->bwp_id > 0) {
      for (int i = 0; i < bwpList->list.count; i++) {
        bwp_downlink = bwpList->list.array[i];
        if(bwp_downlink->bwp_Id == DL_BWP->bwp_id)
          break;
      }
      AssertFatal(bwp_downlink != NULL,"Couldn't find DLBWP corresponding to BWP ID %ld\n", DL_BWP->bwp_id);
      dl_genericParameters = bwp_downlink->bwp_Common->genericParameters;
      DL_BWP->pdsch_Config = bwp_downlink->bwp_Dedicated->pdsch_Config->choice.setup;
      DL_BWP->tdaList_Common = bwp_downlink->bwp_Common->pdsch_ConfigCommon->choice.setup->pdsch_TimeDomainAllocationList;
      configure_ss_coreset(mac,
                           bwp_downlink->bwp_Common->pdcch_ConfigCommon ? bwp_downlink->bwp_Common->pdcch_ConfigCommon->choice.setup : NULL,
                           bwp_downlink->bwp_Dedicated->pdcch_Config ? bwp_downlink->bwp_Dedicated->pdcch_Config->choice.setup : NULL);
      DL_BWP->scs = dl_genericParameters.subcarrierSpacing;
      DL_BWP->cyclicprefix = dl_genericParameters.cyclicPrefix;
      DL_BWP->BWPSize = NRRIV2BW(dl_genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
      DL_BWP->BWPStart = NRRIV2PRBOFFSET(dl_genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
    }
    else {
      DL_BWP->pdsch_Config = spCellConfigDedicated->initialDownlinkBWP->pdsch_Config->choice.setup;
      configure_ss_coreset(mac,
                           NULL,
                           spCellConfigDedicated->initialDownlinkBWP->pdcch_Config ? spCellConfigDedicated->initialDownlinkBWP->pdcch_Config->choice.setup : NULL);
    }

    NR_BWP_Uplink_t *bwp_uplink = NULL;
    const struct NR_UplinkConfig__uplinkBWP_ToAddModList *ubwpList = spCellConfigDedicated->uplinkConfig->uplinkBWP_ToAddModList;
    if (ubwpList)
      UL_BWP->n_ul_bwp = ubwpList->list.count;
    if (ubwpList && UL_BWP->bwp_id > 0) {
      for (int i = 0; i < ubwpList->list.count; i++) {
        bwp_uplink = ubwpList->list.array[i];
        if(bwp_uplink->bwp_Id == UL_BWP->bwp_id)
          break;
      }
      AssertFatal(bwp_uplink != NULL,"Couldn't find ULBWP corresponding to BWP ID %ld\n",UL_BWP->bwp_id);
      ul_genericParameters = bwp_uplink->bwp_Common->genericParameters;
      UL_BWP->tdaList_Common = bwp_uplink->bwp_Common->pusch_ConfigCommon->choice.setup->pusch_TimeDomainAllocationList;
      UL_BWP->pusch_Config = bwp_uplink->bwp_Dedicated->pusch_Config->choice.setup;
      UL_BWP->pucch_Config = bwp_uplink->bwp_Dedicated->pucch_Config->choice.setup;
      UL_BWP->srs_Config = bwp_uplink->bwp_Dedicated->srs_Config->choice.setup;
      UL_BWP->configuredGrantConfig = bwp_uplink->bwp_Dedicated->configuredGrantConfig ? bwp_uplink->bwp_Dedicated->configuredGrantConfig->choice.setup : NULL;
      if (bwp_uplink->bwp_Common->pucch_ConfigCommon)
        UL_BWP->pucch_ConfigCommon = bwp_uplink->bwp_Common->pucch_ConfigCommon->choice.setup;
      if (bwp_uplink->bwp_Common->rach_ConfigCommon) {
        UL_BWP->rach_ConfigCommon = bwp_uplink->bwp_Common->rach_ConfigCommon->choice.setup;
        // Setup the SSB to Rach Occasions mapping according to the config
        build_ssb_to_ro_map(mac);
      }
      UL_BWP->scs = ul_genericParameters.subcarrierSpacing;
      UL_BWP->cyclicprefix = ul_genericParameters.cyclicPrefix;
      UL_BWP->BWPSize = NRRIV2BW(ul_genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
      UL_BWP->BWPStart = NRRIV2PRBOFFSET(ul_genericParameters.locationAndBandwidth, MAX_BWP_SIZE);
    }
    else {
      UL_BWP->pusch_Config = spCellConfigDedicated->uplinkConfig->initialUplinkBWP->pusch_Config->choice.setup;
      UL_BWP->pucch_Config = spCellConfigDedicated->uplinkConfig->initialUplinkBWP->pucch_Config->choice.setup;
      UL_BWP->srs_Config = spCellConfigDedicated->uplinkConfig->initialUplinkBWP->srs_Config->choice.setup;
      UL_BWP->configuredGrantConfig =
          spCellConfigDedicated->uplinkConfig->initialUplinkBWP->configuredGrantConfig ? spCellConfigDedicated->uplinkConfig->initialUplinkBWP->configuredGrantConfig->choice.setup : NULL;
    }
  }
}

void ue_init_config_request(NR_UE_MAC_INST_t *mac, int scs)
{
  int slots_per_frame = nr_slots_per_frame[scs];
  LOG_I(NR_MAC, "Initializing dl and ul config_request. num_slots = %d\n", slots_per_frame);
  mac->dl_config_request = calloc(slots_per_frame, sizeof(*mac->dl_config_request));
  mac->ul_config_request = calloc(slots_per_frame, sizeof(*mac->ul_config_request));
  for (int i = 0; i < slots_per_frame; i++)
    pthread_mutex_init(&(mac->ul_config_request[i].mutex_ul_config), NULL);
}

void nr_rrc_mac_config_req_mib(module_id_t module_id,
                               int cc_idP,
                               NR_MIB_t *mib,
                               int sched_sib)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  AssertFatal(mib, "MIB should not be NULL\n");
  // initialize dl and ul config_request upon first reception of MIB
  mac->mib = mib;    //  update by every reception
  mac->phy_config.Mod_id = module_id;
  mac->phy_config.CC_id = cc_idP;
  if (sched_sib == 1)
    mac->get_sib1 = true;
  else if (sched_sib == 2)
    mac->get_otherSI = true;
  nr_ue_decode_mib(module_id, cc_idP);
}

void nr_rrc_mac_config_req_sib1(module_id_t module_id,
                                int cc_idP,
                                NR_SI_SchedulingInfo_t *si_SchedulingInfo,
                                NR_ServingCellConfigCommonSIB_t *scc)
{
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  AssertFatal(scc, "SIB1 SCC should not be NULL\n");

  updateMACie(mac->tdd_UL_DL_ConfigurationCommon,
              scc->tdd_UL_DL_ConfigurationCommon,
              NR_TDD_UL_DL_ConfigCommon_t);
  updateMACie(mac->si_SchedulingInfo,
              si_SchedulingInfo,
              NR_SI_SchedulingInfo_t);

  config_common_ue_sa(mac, scc, module_id, cc_idP);

  // configure BWP only if it is a SIB1 detection in non connected state
  // not if it is a periodical update of SIB1 (no change of BWP in that case)
  if(mac->state < UE_CONNECTED)
    configure_common_bwp(mac, scc, NULL, NULL, NULL);

  if (!get_softmodem_params()->emulate_l1)
    mac->if_module->phy_config_request(&mac->phy_config);
  mac->phy_config_request_sent = true;
}

void handle_reconfiguration_with_sync(NR_UE_MAC_INST_t *mac,
                                      module_id_t module_id,
                                      int cc_idP,
                                      NR_ServingCellConfig_t *scd,
                                      const NR_ReconfigurationWithSync_t *reconfigurationWithSync)
{

  mac->crnti = reconfigurationWithSync->newUE_Identity;
  LOG_I(NR_MAC, "Configuring CRNTI %x\n", mac->crnti);

  RA_config_t *ra = &mac->ra;
  if (reconfigurationWithSync->rach_ConfigDedicated) {
    AssertFatal(reconfigurationWithSync->rach_ConfigDedicated->present ==
                NR_ReconfigurationWithSync__rach_ConfigDedicated_PR_uplink,
                "RACH on supplementaryUplink not supported\n");
    ra->rach_ConfigDedicated = reconfigurationWithSync->rach_ConfigDedicated->choice.uplink;
  }

  if (reconfigurationWithSync->spCellConfigCommon) {
   NR_ServingCellConfigCommon_t *scc = reconfigurationWithSync->spCellConfigCommon;
    if (scc->physCellId)
      mac->physCellId = *scc->physCellId;
    mac->dmrs_TypeA_Position = scc->dmrs_TypeA_Position;
    updateMACie(mac->tdd_UL_DL_ConfigurationCommon,
                scc->tdd_UL_DL_ConfigurationCommon,
                NR_TDD_UL_DL_ConfigCommon_t);
    config_common_ue(mac, scc, module_id, cc_idP);
    NR_BWP_Id_t *first_dl = NULL;
    NR_BWP_Id_t *first_ul = NULL;
    if (scd) {
      first_dl = scd->firstActiveDownlinkBWP_Id;
      if (scd->uplinkConfig)
        first_ul = scd->uplinkConfig->firstActiveUplinkBWP_Id;
    }
    configure_common_bwp(mac, NULL, scc, first_dl, first_ul);
    mac->current_DL_BWP.bw_tbslbrm = get_dlbw_tbslbrm(mac->current_DL_BWP.initial_BWPSize, scd);
    mac->current_UL_BWP.bw_tbslbrm = get_ulbw_tbslbrm(mac->current_UL_BWP.initial_BWPSize, scd);
  }

  mac->state = UE_NOT_SYNC;
  ra->ra_state = RA_UE_IDLE;
  nr_ue_mac_default_configs(mac);

  if (!get_softmodem_params()->emulate_l1) {
    mac->synch_request.Mod_id = module_id;
    mac->synch_request.CC_id = cc_idP;
    mac->synch_request.synch_req.target_Nid_cell = mac->physCellId;
    mac->if_module->synch_request(&mac->synch_request);
    mac->if_module->phy_config_request(&mac->phy_config);
    mac->phy_config_request_sent = true;
  }
}

void configure_physicalcellgroup(NR_UE_MAC_INST_t *mac,
                                 const NR_PhysicalCellGroupConfig_t *phyConfig)
{
  mac->pdsch_HARQ_ACK_Codebook = phyConfig->pdsch_HARQ_ACK_Codebook;
  mac->harq_ACK_SpatialBundlingPUCCH = phyConfig->harq_ACK_SpatialBundlingPUCCH ? true : false;
  mac->harq_ACK_SpatialBundlingPUSCH = phyConfig->harq_ACK_SpatialBundlingPUSCH ? true : false;

  NR_P_Max_t *p_NR_FR1 = phyConfig->p_NR_FR1;
  NR_P_Max_t *p_UE_FR1 = phyConfig->ext1 ?
                         phyConfig->ext1->p_UE_FR1 :
                         NULL;
  if (p_NR_FR1 == NULL)
    mac->p_Max_alt = p_UE_FR1 == NULL ? INT_MIN : *p_UE_FR1;
  else
    mac->p_Max_alt = p_UE_FR1 == NULL ? *p_NR_FR1 :
                                        (*p_UE_FR1 < *p_NR_FR1 ?
                                        *p_UE_FR1 : *p_NR_FR1);
}

void nr_rrc_mac_config_req_cg(module_id_t module_id,
                              int cc_idP,
                              NR_CellGroupConfig_t *cell_group_config)
{
  LOG_I(MAC,"Applying CellGroupConfig from gNodeB\n");
  AssertFatal(cell_group_config, "CellGroupConfig should not be NULL\n");
  NR_UE_MAC_INST_t *mac = get_mac_inst(module_id);
  mac->cg = cell_group_config;

  if (cell_group_config->mac_CellGroupConfig) {
    // TODO handle MAC-CellGroupConfig
  }

  if (cell_group_config->physicalCellGroupConfig)
    configure_physicalcellgroup(mac, cell_group_config->physicalCellGroupConfig);

  if (cell_group_config->spCellConfig) {
    NR_SpCellConfig_t *spCellConfig = cell_group_config->spCellConfig;
    NR_ServingCellConfig_t *scd = spCellConfig->spCellConfigDedicated;
    mac->servCellIndex = spCellConfig->servCellIndex ? *spCellConfig->servCellIndex : 0;
    if (spCellConfig->reconfigurationWithSync) {
      LOG_A(NR_MAC, "Received reconfigurationWithSync\n");
      handle_reconfiguration_with_sync(mac, module_id, cc_idP, scd, spCellConfig->reconfigurationWithSync);
    }
    if (scd) {
      mac->crossCarrierSchedulingConfig = scd->crossCarrierSchedulingConfig;
      configure_current_BWP(mac, scd);
    }
  }

  if (!mac->dl_config_request || !mac->ul_config_request)
    ue_init_config_request(mac, mac->current_DL_BWP.scs);
}
