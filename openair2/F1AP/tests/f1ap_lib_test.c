#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "common/utils/assertions.h"

#include "f1ap_messages_types.h"
#include "F1AP_F1AP-PDU.h"

#include "f1ap_lib_extern.h"
#include "lib/f1ap_interface_management.h"

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  printf("detected error at %s:%d:%s: %s\n", file, line, function, s);
  abort();
}

static F1AP_F1AP_PDU_t *f1ap_encode_decode(const F1AP_F1AP_PDU_t *enc_pdu)
{
  //xer_fprint(stdout, &asn_DEF_F1AP_F1AP_PDU, enc_pdu);

  DevAssert(enc_pdu != NULL);
  char errbuf[1024];
  size_t errlen = sizeof(errbuf);
  int ret = asn_check_constraints(&asn_DEF_F1AP_F1AP_PDU, enc_pdu, errbuf, &errlen);
  AssertFatal(ret == 0, "asn_check_constraints() failed: %s\n", errbuf);

  uint8_t msgbuf[16384];
  asn_enc_rval_t enc = aper_encode_to_buffer(&asn_DEF_F1AP_F1AP_PDU, NULL, enc_pdu, msgbuf, sizeof(msgbuf));
  AssertFatal(enc.encoded > 0, "aper_encode_to_buffer() failed\n");

  F1AP_F1AP_PDU_t *dec_pdu = NULL;
  asn_codec_ctx_t st = {.max_stack_size = 100 * 1000};
  asn_dec_rval_t dec = aper_decode(&st, &asn_DEF_F1AP_F1AP_PDU, (void **)&dec_pdu, msgbuf, enc.encoded, 0, 0);
  AssertFatal(dec.code == RC_OK, "aper_decode() failed\n");

  //xer_fprint(stdout, &asn_DEF_F1AP_F1AP_PDU, dec_pdu);
  return dec_pdu;
}

static void f1ap_msg_free(F1AP_F1AP_PDU_t *pdu)
{
  ASN_STRUCT_FREE(asn_DEF_F1AP_F1AP_PDU, pdu);
}

/**
 * @brief Test Initial UL RRC Message Transfer encoding/decoding
 */
static void test_initial_ul_rrc_message_transfer(void)
{
  f1ap_plmn_t plmn = { .mcc = 208, .mnc = 95, .mnc_digit_length = 2 };
  uint8_t rrc[] = "RRC Container";
  uint8_t du2cu[] = "DU2CU Container";
  f1ap_initial_ul_rrc_message_t orig = {
    .gNB_DU_ue_id = 12,
    .plmn = plmn,
    .nr_cellid = 135,
    .crnti = 0x1234,
    .rrc_container = rrc,
    .rrc_container_length = sizeof(rrc),
    .du2cu_rrc_container = du2cu,
    .du2cu_rrc_container_length = sizeof(du2cu),
    .transaction_id = 2,
  };

  F1AP_F1AP_PDU_t *f1enc = encode_initial_ul_rrc_message_transfer(&orig);
  F1AP_F1AP_PDU_t *f1dec = f1ap_encode_decode(f1enc);
  f1ap_msg_free(f1enc);

  f1ap_initial_ul_rrc_message_t decoded;
  bool ret = decode_initial_ul_rrc_message_transfer(f1dec, &decoded);
  AssertFatal(ret, "decode_initial_ul_rrc_message_transfer(): could not decode message\n");
  f1ap_msg_free(f1dec);

  ret = eq_initial_ul_rrc_message_transfer(&orig, &decoded);
  AssertFatal(ret, "eq_initial_ul_rrc_message_transfer(): decoded message doesn't match\n");
  free_initial_ul_rrc_message_transfer(&decoded);

  f1ap_initial_ul_rrc_message_t cp = cp_initial_ul_rrc_message_transfer(&orig);
  ret = eq_initial_ul_rrc_message_transfer(&orig, &cp);
  AssertFatal(ret, "eq_initial_ul_rrc_message_transfer(): copied message doesn't match\n");
  free_initial_ul_rrc_message_transfer(&cp);
}

/**
 * @brief Test DL RRC Message Transfer encoding/decoding
 */
static void test_dl_rrc_message_transfer(void)
{
  uint8_t *rrc = (uint8_t *)calloc(strlen("RRC Container") + 1, sizeof(uint8_t));
  uint32_t *old_gNB_DU_ue_id = calloc(1, sizeof(uint32_t));
  AssertFatal(rrc != NULL && old_gNB_DU_ue_id != NULL, "out of memory\n");
  strcpy((char *)rrc, "RRC Container");

  f1ap_dl_rrc_message_t orig = {
    .gNB_DU_ue_id = 12,
    .gNB_CU_ue_id = 12,
    .old_gNB_DU_ue_id = old_gNB_DU_ue_id,
    .srb_id = 1,
    .rrc_container = rrc,
    .rrc_container_length = sizeof(rrc),
  };

  F1AP_F1AP_PDU_t *f1enc = encode_dl_rrc_message_transfer(&orig);
  F1AP_F1AP_PDU_t *f1dec = f1ap_encode_decode(f1enc);
  f1ap_msg_free(f1enc);

  f1ap_dl_rrc_message_t decoded = {0};
  bool ret = decode_dl_rrc_message_transfer(f1dec, &decoded);
  AssertFatal(ret, "decode_initial_ul_rrc_message_transfer(): could not decode message\n");
  f1ap_msg_free(f1dec);

  ret = eq_dl_rrc_message_transfer(&orig, &decoded);
  AssertFatal(ret, "eq_dl_rrc_message_transfer(): decoded message doesn't match\n");
  free_dl_rrc_message_transfer(&decoded);

  f1ap_dl_rrc_message_t cp = cp_dl_rrc_message_transfer(&orig);
  ret = eq_dl_rrc_message_transfer(&orig, &cp);
  AssertFatal(ret, "eq_dl_rrc_message_transfer(): copied message doesn't match\n");
  free_dl_rrc_message_transfer(&orig);
  free_dl_rrc_message_transfer(&cp);
}

/**
 * @brief Test UL RRC Message Transfer encoding/decoding
 */
static void test_ul_rrc_message_transfer(void)
{
  uint8_t rrc[] = "RRC Container";

  f1ap_ul_rrc_message_t orig = {
    .gNB_DU_ue_id = 12,
    .gNB_CU_ue_id = 12,
    .srb_id = 1,
    .rrc_container = rrc,
    .rrc_container_length = sizeof(rrc),
  };

  F1AP_F1AP_PDU_t *f1enc = encode_ul_rrc_message_transfer(&orig);
  F1AP_F1AP_PDU_t *f1dec = f1ap_encode_decode(f1enc);
  f1ap_msg_free(f1enc);

  f1ap_ul_rrc_message_t decoded = {0};
  bool ret = decode_ul_rrc_message_transfer(f1dec, &decoded);
  AssertFatal(ret, "decode_initial_ul_rrc_message_transfer(): could not decode message\n");
  f1ap_msg_free(f1dec);

  ret = eq_ul_rrc_message_transfer(&orig, &decoded);
  AssertFatal(ret, "eq_dl_rrc_message_transfer(): decoded message doesn't match\n");
  free_ul_rrc_message_transfer(&decoded);

  f1ap_ul_rrc_message_t cp = cp_ul_rrc_message_transfer(&orig);
  ret = eq_ul_rrc_message_transfer(&orig, &cp);
  AssertFatal(ret, "eq_dl_rrc_message_transfer(): copied message doesn't match\n");
  free_ul_rrc_message_transfer(&cp);
}

/**
 * @brief Test F1AP Setup Request Encoding/Decoding
 */
static void test_f1ap_setup_request(void)
{
  /* allocate memory */
  /* gNB_DU_name */
  uint8_t *gNB_DU_name = (uint8_t *)calloc(strlen("OAI DU") + 1, sizeof(uint8_t));
  AssertFatal(gNB_DU_name != NULL, "out of memory\n");
  strcpy((char *)gNB_DU_name, "OAI DU");
  /* sys_info */
  uint8_t *mib = calloc(3, sizeof(uint8_t));
  uint8_t *sib1 = calloc(3, sizeof(uint8_t));
  f1ap_gnb_du_system_info_t sys_info = {
      .mib_length = 3,
      .mib = mib,
      .sib1_length = 3,
      .sib1 = sib1,
  };
  /* measurement_timing_information */
  int measurement_timing_config_len = strlen("0") + 1;
  uint8_t *measurement_timing_information = (uint8_t *)calloc(measurement_timing_config_len, sizeof(uint8_t));
  AssertFatal(measurement_timing_information != NULL, "out of memory\n");
  strcpy((char *)measurement_timing_information, "0");
  /* TAC */
  uint32_t *tac = calloc(1, sizeof(uint32_t));
  /* info */
  f1ap_served_cell_info_t info = {
      .mode = F1AP_MODE_TDD,
      .tdd.freqinfo.arfcn = 640000,
      .tdd.freqinfo.band = 78,
      .tdd.tbw.nrb = 66,
      .tdd.tbw.scs = 1,
      .measurement_timing_config_len = measurement_timing_config_len,
      .measurement_timing_config = measurement_timing_information,
      .nr_cellid = 123456,
      .plmn.mcc = 1,
      .plmn.mnc = 1,
      .plmn.mnc_digit_length = 3,
      .num_ssi = 1,
      .nssai[0].sst = 1,
      .nssai[0].sd = 1,
      .tac = tac,
  };
  /* create message */
  f1ap_setup_req_t orig = {
      .gNB_DU_id = 1,
      .gNB_DU_name = (char *)gNB_DU_name,
      .num_cells_available = 1,
      .transaction_id = 2,
      .rrc_ver[0] = 12,
      .rrc_ver[1] = 34,
      .rrc_ver[2] = 56,
      .cell[0].info = info,
  };
  orig.cell[0].sys_info = calloc(1, sizeof(*orig.cell[0].sys_info));
  *orig.cell[0].sys_info = sys_info;
  F1AP_F1AP_PDU_t *f1enc = encode_f1ap_setup_request(&orig);
  F1AP_F1AP_PDU_t *f1dec = f1ap_encode_decode(f1enc);
  f1ap_msg_free(f1enc);

  f1ap_setup_req_t decoded = {0};
  bool ret = decode_f1ap_setup_request(f1dec, &decoded);
  AssertFatal(ret, "decode_f1ap_setup_request(): could not decode message\n");
  f1ap_msg_free(f1dec);

  ret = eq_f1ap_setup_request(&orig, &decoded);
  AssertFatal(ret, "eq_f1ap_setup_request(): decoded message doesn't match\n");
  free_f1ap_setup_request(&decoded);

  f1ap_setup_req_t cp = cp_f1ap_setup_request(&orig);
  ret = eq_f1ap_setup_request(&orig, &cp);
  AssertFatal(ret, "eq_f1ap_setup_request(): copied message doesn't match\n");
  free_f1ap_setup_request(&cp);
  free_f1ap_setup_request(&orig);
}

int main()
{
  test_initial_ul_rrc_message_transfer();
  test_dl_rrc_message_transfer();
  test_ul_rrc_message_transfer();
  test_f1ap_setup_request();
  return 0;
}