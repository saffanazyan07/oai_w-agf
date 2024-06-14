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

#include "nr_refsig.h"

uint32_t* gold_cache(uint32_t key, int length)
{
  typedef struct {
    int key;
    int length;
    int usage;
  } gold_cache_t;
  
  static __thread uint32_t *table=NULL;
  static __thread uint32_t tblSz=0;
  static __thread int calls = 0;
  // align for AVX512
  const int roundedHeaderSz = (((sizeof(gold_cache_t) + 63) / 64) * 64) / sizeof(*table);
  const int grain = 64 / sizeof(*table);
  length = ((length + grain - 1) / grain) * grain;
  calls++;
  uint32_t *ptr = table;
  // check if already cached
  for (; ptr < table+tblSz ; ptr+=roundedHeaderSz) {
    gold_cache_t *tbl=(gold_cache_t*) ptr;
    if (tbl->length && tbl->key == key) {
      tbl->usage++;
      return ptr+roundedHeaderSz;
    }
    if (!tbl->length)
      break;
    ptr+=tbl->length;
  }

  // Allocate, also reorder to have the most frequent first, so the cache search is optimized
  if (!ptr || ptr > table + tblSz - (2 * roundedHeaderSz + length) || calls > 100000) {
    uint32_t *old = table;
    uint oldSz = tblSz;
    tblSz += max(length + 2 * roundedHeaderSz, PAGE_SIZE / sizeof(*table));
    int ret = posix_memalign((void **)&table, 64, tblSz * sizeof(*table));
    AssertFatal(ret == 0, "No more memory");
    ;
    LOG_I(PHY, "Increase gold sequence table to %lu pages of memory\n", tblSz * sizeof(*table) / PAGE_SIZE);
    int maxUsage;
    uint32_t *currentTmp = table;
    do {
      maxUsage=0;
      gold_cache_t *entryToCopy=NULL;
      for (uint32_t *searchmax = old; searchmax < old + oldSz; searchmax += roundedHeaderSz) {
        gold_cache_t *tbl = (gold_cache_t *)searchmax;
        if (!tbl->length)
          break;
        if (tbl->usage > maxUsage) {
          maxUsage = tbl->usage;
          entryToCopy = tbl;
        }
        searchmax += tbl->length;
      }
      if (maxUsage) {
	memcpy(currentTmp, entryToCopy, (roundedHeaderSz+ entryToCopy->length)*sizeof(*table));
	currentTmp+=roundedHeaderSz+ entryToCopy->length;
	entryToCopy->usage=0;
      }
    } while (maxUsage);
    const uint usedSz = currentTmp - table;
    memset(table + usedSz, 0, (tblSz - usedSz) * sizeof(*table));
    free(old);
  }

  // We will add a new entry
  uint32_t * firstFree;
  for (firstFree=table; firstFree < table+tblSz ; firstFree+=roundedHeaderSz) {
    gold_cache_t *tbl=(gold_cache_t*) firstFree;
    if (!tbl->length)
      break;
    firstFree+=tbl->length;
  }

  gold_cache_t *new=(gold_cache_t*) firstFree;
  *new= (gold_cache_t) {.key = key,  .length = length,.usage = 1};
  unsigned int x1 = 0, x2 = key;
  uint32_t *sequence=firstFree+roundedHeaderSz;
  *sequence++ = lte_gold_generic(&x1, &x2, 1);
  for (int n = 1; n < length; n++)
    *sequence++ = lte_gold_generic(&x1, &x2, 0);
  LOG_W(PHY, "created a gold sequence, start %d; len %d\n", key, length);
  return firstFree+roundedHeaderSz;
}

uint32_t* nr_gold_pbch(int Lmax, int Nid, int n_hf, int l)
{
  int i_ssb = l & (Lmax - 1);
  int i_ssb2 = i_ssb + (n_hf << 2);
  uint32_t x2 = (1 << 11) * (i_ssb2 + 1) * ((Nid >> 2) + 1) + (1 << 6) * (i_ssb2 + 1) + (Nid & 3);
  return gold_cache(x2, NR_PBCH_DMRS_LENGTH_DWORD);
}

uint32_t* nr_gold_pdcch(int N_RB_DL, int symbols_per_slot, unsigned short nid, int ns, int l)
{
  int pdcch_dmrs_init_length = (((N_RB_DL << 1) * 3) >> 5) + 1;
  uint64_t x2tmp0 = (((uint64_t)symbols_per_slot * ns + l + 1) * ((nid << 1) + 1));
  x2tmp0 <<= 17;
  x2tmp0 += (nid << 1);
  uint32_t x2 = x2tmp0 % (1U << 31); // cinit
  LOG_D(PHY, "PDCCH DMRS slot %d, symb %d, Nid %d, x2 %x\n", ns, l, nid, x2);
  return gold_cache(x2, pdcch_dmrs_init_length);
}

uint32_t* nr_gold_pdsch(int N_RB_DL, int symbols_per_slot, int nid, int nscid, int slot, int symbol)
{
  int pdsch_dmrs_init_length = ((N_RB_DL * 24) >> 5) + 1;
  uint64_t x2tmp0 = (((uint64_t)symbols_per_slot * slot + symbol + 1) * (((uint64_t)nid << 1) + 1)) << 17;
  uint32_t x2 = (x2tmp0 + (nid << 1) + nscid) % (1U << 31); // cinit
  LOG_D(PHY, "UE DMRS slot %d, symb %d, nscid %d, x2 %x\n", slot, symbol, nscid, x2);
  return gold_cache(x2, pdsch_dmrs_init_length);
}

uint32_t* nr_gold_pusch(int N_RB_UL, int symbols_per_slot, int Nid, int nscid, int ns, int l)
{
  int pusch_dmrs_init_length = ((N_RB_UL * 12) >> 5) + 1;
  uint64_t temp_x2 = ((1ULL << 17) * (symbols_per_slot * ns + l + 1) * ((Nid << 1) + 1) + ((Nid << 1) + nscid));
  uint32_t x2 = temp_x2 % (1U << 31);
  LOG_D(PHY, "DMRS slot %d, symb %d, nscid %d, nid %d, x2 %x\n", ns, l, nscid, Nid, x2);
  return gold_cache(x2, pusch_dmrs_init_length);
}

uint32_t* nr_gold_csi_rs(const NR_DL_FRAME_PARMS* fp, int slot, int symb, uint32_t Nid)
{
  int csi_dmrs_init_length =  ((fp->N_RB_DL<<4)>>5)+1;
  uint32_t x2 = ((1ULL << 10) * (fp->symbols_per_slot * slot + symb + 1) * ((Nid << 1) + 1) + (Nid));
  return gold_cache(x2, csi_dmrs_init_length);
}

uint32_t* nr_gold_prs(int Nid, int slotNum, int symNum)
{
  LOG_I(PHY, "Initialised NR-PRS sequence for PCI %d\n", Nid);
  // initial x2 for prs as ts138.211
  uint32_t pow22 = 1 << 22;
  uint32_t pow10 = 1 << 10;
  uint32_t c_init1 = pow22 * ceil(Nid / 1024);
  uint32_t c_init2 = pow10 * (slotNum + symNum + 1) * (2 * (Nid % 1024) + 1);
  uint32_t c_init3 = Nid % 1024;
  uint32_t x2 = c_init1 + c_init2 + c_init3;
  return gold_cache(x2, NR_MAX_PRS_INIT_LENGTH_DWORD);
}
