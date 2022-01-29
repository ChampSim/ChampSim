// Note that some variables and functions are defined at kpcp_util.cc

#include "kpcp.h"

#include "cache.h"

#define PF_THRESHOLD 25
#define FILL_THRESHOLD 75
#define LOOKAHEAD_ON
#define GC_WIDTH 10
#define GC_MAX ((1 << GC_WIDTH) - 1)

// defined at kpcp_util.cc
/*
SIGNATURE_TABLE L2_ST[NUM_CPUS][L2_ST_SET][L2_ST_WAY];
PATTERN_TABLE L2_PT[NUM_CPUS][L2_PT_SET][L2_PT_WAY];
GLOBAL_HISTORY_REGISTER L2_GHR[NUM_CPUS][L2_GHR_TRACK];

int L2_ST_access[NUM_CPUS], L2_ST_hit[NUM_CPUS], L2_ST_invalid[NUM_CPUS],
L2_ST_miss[NUM_CPUS]; int L2_PT_access[NUM_CPUS], L2_PT_hit[NUM_CPUS],
L2_PT_invalid[NUM_CPUS], L2_PT_miss[NUM_CPUS]; int
l2_sig_dist[NUM_CPUS][1<<SIG_LENGTH];
*/

int num_pf[NUM_CPUS], curr_conf[NUM_CPUS], curr_delta[NUM_CPUS], MAX_CONF[NUM_CPUS];
int out_of_page[NUM_CPUS], not_enough_conf[NUM_CPUS];
int pf_delta[NUM_CPUS][L2C_MSHR_SIZE], PF_inflight[NUM_CPUS];
int spp_pf_issued[NUM_CPUS], spp_pf_useful[NUM_CPUS], spp_pf_useless[NUM_CPUS];
int useful_depth[NUM_CPUS][L2C_MSHR_SIZE], useless_depth[NUM_CPUS][L2C_MSHR_SIZE];
int conf_counter[NUM_CPUS];

int PF_check(uint32_t cpu, int signature, int curr_block);
int st_prime = L2_ST_PRIME, pt_prime = L2_PT_PRIME;

class PF_buffer
{
public:
  int delta, signature, conf, depth;

  PF_buffer()
  {
    delta = 0;
    signature = 0;
    conf = 0;
    depth = 0;
  };
};
PF_buffer pf_buffer[NUM_CPUS][L2C_MSHR_SIZE];

void CACHE::prefetcher_initialize()
{
  std::cout << NAME << " Signature Path Prefetcher" << std::endl;

  spp_pf_issued[cpu] = 0;
  spp_pf_useful[cpu] = 0;
  spp_pf_useless[cpu] = 0;

  for (int i = 0; i < L2C_MSHR_SIZE; i++) {
    useful_depth[cpu][i] = 0;
    useless_depth[cpu][i] = 0;
  }

  for (int i = 0; i < L2_ST_SET; i++) {
    for (int j = 0; j < L2_ST_WAY; j++)
      L2_ST[cpu][i][j].lru = j;
  }

  for (int i = 0; i < L2_GHR_TRACK; i++)
    L2_GHR[cpu][i].lru = i;

  conf_counter[cpu] = 0;
}

void GHR_update(uint32_t cpu, int signature, int path_conf, int last_block, int oop_delta)
{
  int match;
  for (match = 0; match < L2_GHR_TRACK; match++) {
    if (L2_GHR[cpu][match].signature == signature) { // Hit

      // Update metadata
      L2_GHR[cpu][match].signature = signature;
      L2_GHR[cpu][match].path_conf = path_conf;
      L2_GHR[cpu][match].last_block = last_block;
      L2_GHR[cpu][match].oop_delta = oop_delta;

      break;
    }
  }

  if (match == L2_GHR_TRACK) {
    for (match = 0; match < L2_GHR_TRACK; match++) {
      if (L2_GHR[cpu][match].signature == 0) { // Invalid

        // Update metadata
        L2_GHR[cpu][match].signature = signature;
        L2_GHR[cpu][match].path_conf = path_conf;
        L2_GHR[cpu][match].last_block = last_block;
        L2_GHR[cpu][match].oop_delta = oop_delta;

        break;
      }
    }
  }

  if (match == L2_GHR_TRACK) { // Miss

    // Search for LRU victim
    int max_idx = -1;
    int max_lru = 0;
    for (match = 0; match < L2_GHR_TRACK; match++) {
      if (L2_GHR[cpu][match].lru >= max_lru) {
        max_idx = match;
        max_lru = L2_GHR[cpu][match].lru;
      }
    }
    match = max_idx;

    // Update metadata
    L2_GHR[cpu][match].signature = signature;
    L2_GHR[cpu][match].path_conf = path_conf;
    L2_GHR[cpu][match].last_block = last_block;
    L2_GHR[cpu][match].oop_delta = oop_delta;
  }

  // Update LRU
  int position = L2_GHR[cpu][match].lru;
  for (int i = 0; i < L2_GHR_TRACK; i++) {
    if (L2_GHR[cpu][match].lru < position)
      L2_GHR[cpu][match].lru++;
  }
  L2_GHR[cpu][match].lru = 0;

  return;
}

int check_same_page(int curr_block, int delta)
{
  if ((0 <= (curr_block + delta)) && ((curr_block + delta) <= 63))
    return 1;
  else
    return 0;
}

// Check prefetch candidate
int PF_check(uint32_t cpu, int signature, int curr_block)
{
  int l2_pt_idx = signature % pt_prime; // L2_PT_PRIME;
  PATTERN_TABLE* table = L2_PT[cpu][l2_pt_idx];
  int pf_max = 0, pf_idx = -1, conf_max = 100, temp_conf = 100;

  if (table[0].c_sig) // This signature was updated at least once
  {
    // Search for prefetch candidates

    for (int i = 0; i < L2_PT_WAY; i++) {
      temp_conf = (100 * table[i].c_delta) / table[0].c_sig;

      if (temp_conf >= PF_THRESHOLD) // This delta entry has enough confidence
      {
        if (check_same_page(curr_block,
                            table[i].delta)) // Safe to prefetch in page boundary
        {
          pf_buffer[cpu][num_pf[cpu]].delta = table[i].delta;
          pf_buffer[cpu][num_pf[cpu]].signature = signature;
          pf_buffer[cpu][num_pf[cpu]].depth = 1;
          pf_buffer[cpu][num_pf[cpu]].conf = temp_conf;

          if (warmup_complete[cpu])
            L2_PF_DEBUG(printf("PF_buffer cpu: %d idx: %d delta: %d signature: "
                               "%x depth: %d conf: %d\n",
                               cpu, 0, pf_buffer[cpu][num_pf[cpu]].delta, pf_buffer[cpu][num_pf[cpu]].signature, pf_buffer[cpu][num_pf[cpu]].depth,
                               pf_buffer[cpu][num_pf[cpu]].conf));

          num_pf[cpu]++;
        } else // Store it in the GHR
        {
          out_of_page[cpu]++;
          if (warmup_complete[cpu])
            L2_PF_DEBUG(printf("PT_sig: %4x PF_check OOP high_curr_conf: %d  way: %d  "
                               "delta: %d  counter: %d / %d  MAX_CONF: %d\n",
                               signature, temp_conf, i, table[i].delta, table[i].c_delta, table[0].c_sig, MAX_CONF[cpu]));

#ifdef L2_GHR_ON
          GHR_update(cpu, signature, temp_conf, curr_block, table[i].delta);
#endif
        }

        // Track the maximum counter regardless of page boundary
        if (pf_max < table[i].c_delta) {
          pf_max = table[i].c_delta;
          pf_idx = i;
          conf_max = temp_conf;
        }
        if (warmup_complete[cpu])
          L2_PF_DEBUG(printf("PT_sig: %4x PF_check candidate  high_curr_conf: %d  way: "
                             "%d  delta: %d  counter: %d / %d  MAX_CONF: %d\n",
                             signature, temp_conf, i, table[i].delta, table[i].c_delta, table[i].c_sig, MAX_CONF[cpu]));
      } else {
        not_enough_conf[cpu]++;
        if (warmup_complete[cpu])
          L2_PF_DEBUG(printf("PT_sig: %4x PF_check no candidate  low_curr_conf: %d  "
                             "way: %d  stride: %d  counter: %d / %d  MAX_CONF: %d\n",
                             signature, temp_conf, i, table[i].delta, table[i].c_delta, table[i].c_sig, MAX_CONF[cpu]));
      }
    }

    // Update the path confidence
    if (pf_idx >= 0) {
      curr_conf[cpu] = conf_max;
      curr_delta[cpu] = table[pf_idx].delta;
    } else {
      curr_conf[cpu] = 0;
      curr_delta[cpu] = 0;
    }
  } else {
    curr_conf[cpu] = 0;
    curr_delta[cpu] = 0;
  }

#ifdef LOOKAHEAD_ON
  int la_signature = signature, la_pf_max, la_pf_idx, LA_idx;
  int last_delta = 0;

  if (curr_conf[cpu] >= PF_THRESHOLD) {
    do {
      la_signature = get_new_signature(la_signature, curr_delta[cpu] - last_delta);
      la_pf_max = 0;
      la_pf_idx = -1;
      LA_idx = la_signature % pt_prime; // L2_PT_PRIME;
      table = L2_PT[cpu][LA_idx];
      if (table[0].c_sig) // This signature was updated at least once
      {
        // Search for lookahead prefetch candidates

        for (int i = 0; i < L2_PT_WAY; i++) {
          // Calculate path confidence
          temp_conf = curr_conf[cpu] * table[i].c_delta / table[0].c_sig * MAX_CONF[cpu] / 100;

          if (temp_conf >= PF_THRESHOLD) // This delta entry has enough confidence
          {
            // Track the maximum counter regardless of page boundary
            if (la_pf_max < table[i].c_delta) {
              la_pf_max = table[i].c_delta;
              la_pf_idx = i;
              conf_max = temp_conf;
            }
            if (warmup_complete[cpu])
              L2_PF_DEBUG(printf("PT_sig: %4x LA_check candidate  high_curr_conf: %d  way: %d "
                                 " stride: %d  counter: %d / %d  MAX_CONF: %d\n",
                                 la_signature, temp_conf, i, table[i].delta, table[i].c_delta, table[0].c_sig, MAX_CONF[cpu]));
          } else {
            not_enough_conf[cpu]++;
            if (warmup_complete[cpu])
              L2_PF_DEBUG(printf("PT_sig: %4x LA_check no candidate  low_curr_conf: %d  way: "
                                 "%d  stride: %d  counter: %d / %d  MAX_CONF: %d\n",
                                 la_signature, temp_conf, i, table[i].delta, table[i].c_delta, table[0].c_sig, MAX_CONF[cpu]));
          }
        }

        // Update the path confidence
        if (la_pf_idx >= 0) {
          if (num_pf[cpu] < L2C_MSHR_SIZE) {
            // Safe to prefetch in page boundary
            if (check_same_page(curr_block, curr_delta[cpu] + table[la_pf_idx].delta)) {
              if (curr_delta[cpu] + table[la_pf_idx].delta) {
                pf_buffer[cpu][num_pf[cpu]].delta = curr_delta[cpu] + table[la_pf_idx].delta;
                pf_buffer[cpu][num_pf[cpu]].signature = la_signature;
                pf_buffer[cpu][num_pf[cpu]].depth = num_pf[cpu] + 1;
                pf_buffer[cpu][num_pf[cpu]].conf = conf_max;
                num_pf[cpu]++;
              }
            }

            last_delta = curr_delta[cpu];
            curr_conf[cpu] = conf_max;
            curr_delta[cpu] += table[la_pf_idx].delta;
          } else {
            last_delta = curr_delta[cpu];
            curr_conf[cpu] = 0;
            curr_delta[cpu] = 0;
          }
        } else {
          last_delta = curr_delta[cpu];
          curr_conf[cpu] = 0;
          curr_delta[cpu] = 0;
        }
      } else {
        last_delta = curr_delta[cpu];
        curr_conf[cpu] = 0;
        curr_delta[cpu] = 0;
      }
    } while (curr_conf[cpu] >= PF_THRESHOLD);
  }
#endif

  return 0;
}

// defined at kpcp_util.cc
/*
unsigned int get_new_signature(unsigned int old_signature, int curr_delta)
{
    if (curr_delta == 0)
        return old_signature;

    unsigned int new_signature = 0;
    int sig_delta = curr_delta;
    if (sig_delta < 0)
        sig_delta = 64 + curr_delta*(-1);
    new_signature = ((old_signature << SIG_SHIFT) ^ sig_delta) & SIG_MASK;
    if (new_signature == 0)
    {
        //printf("old_signature: %x  SIG_SHIFT: %d  sig_delta: %d  SIG_LENGTH:
%d\n", old_signature, SIG_SHIFT, sig_delta, SIG_LENGTH); if (sig_delta) return
sig_delta; else return old_signature;
    }
    return new_signature;
}

// Update signature table
int L2_ST_update(uint32_t cpu, uint64_t addr)
{
    uint64_t curr_page = addr >> LOG2_PAGE_SIZE;
    int tag = curr_page & 0xFFFF,
        hit = 0, match = -1,
        L2_ST_idx = curr_page % st_prime, //L2_ST_PRIME,
        curr_block = (addr >> LOG2_BLOCK_SIZE) & 0x3F;
    SIGNATURE_TABLE *table = L2_ST[cpu][L2_ST_idx];
    int delta_buffer = 0, sig_buffer = 0;

    for (match=0; match<L2_ST_WAY; match++) {
        if (table[match].valid && (table[match].tag == tag)) { // Hit
                        delta_buffer = curr_block - table[match].last_block; //
Buffer current delta sig_buffer = table[match].signature; // Buffer old
signature

            if (table[match].signature == 0) { // First hit in L2_ST
                // We cannot associate delta pattern with signature when we see
"the first hit in L2_ST"
                // At this point, all we know about this page is "the first
accessed offset"
                // We don't have any delta information that can be a part of
signature
                // In other words, the first offset does not update PT

                int sig_delta = curr_block - table[match].last_block;
                if (sig_delta < 0)
                    sig_delta = 64 + (curr_block -
table[match].last_block)*(-1); table[match].signature = sig_delta & SIG_MASK; //
This is the first signature table[match].first_hit = 1;
                l2_sig_dist[cpu][table[match].signature]++;

                if (warmup_complete[cpu])
                L2_PF_DEBUG(printf("ST_hit_first cpu: %d cl_addr: %lx page: %lx
block: %d init_sig: %x delta: %d\n", cpu, addr >> LOG2_BLOCK_SIZE, curr_page,
curr_block, table[match].signature, delta_buffer));
            }
            else {
                hit = 1;
                table[match].first_hit = 0;

                if (delta_buffer) {
                    // This is non-speculative information tracked from actual
L2 cache demand
                    // Now, the old signature will be associated with current
delta L2_PT_update(cpu, sig_buffer, delta_buffer);
                }
                else
                    break;

                if (warmup_complete[cpu])
                L2_PF_DEBUG(printf("ST_hit cpu: %d cl_addr: %lx page: %lx block:
%d old_sig: %x delta: %d\n", cpu, addr >> LOG2_BLOCK_SIZE, curr_page,
curr_block, sig_buffer, delta_buffer));

                // Update signature
                int new_signature = get_new_signature(sig_buffer, delta_buffer);
                table[match].signature = new_signature;
                l2_sig_dist[cpu][table[match].signature]++;
            }

                        // Update last_block
                        table[match].last_block = curr_block;
            L2_ST_hit[cpu]++; L2_ST_access[cpu]++;
            break;
        }
    }

    if (match == L2_ST_WAY) {
        for (match=0; match<L2_ST_WAY; match++) {
            if (table[match].valid == 0) { // Invalid
                // Update metadata
                table[match].valid = 1;
                table[match].tag = tag;
                table[match].signature = 0;
                table[match].first_hit = 0;
                table[match].last_block = curr_block;
                L2_ST_invalid[cpu]++; L2_ST_access[cpu]++;

                if (warmup_complete[cpu])
                L2_PF_DEBUG(printf("ST_invalid cpu: %d cl_addr: %lx page: %lx
block: %d\n", cpu, addr >> LOG2_BLOCK_SIZE, curr_page, curr_block)); break;
            }
        }
    }

    if (match == L2_ST_WAY) { // Miss
        // Search for LRU victim
        for (match=0; match<L2_ST_WAY; match++) {
            if (table[match].lru == (L2_ST_WAY-1))
                break;
        }

        // Update metadata
        table[match].valid = 1;
        table[match].tag = tag;
        table[match].signature = 0;
        table[match].first_hit = 0;
        table[match].last_block = curr_block;

        for (int i=0; i<64; i++) {
            table[match].l2_pf[i] = 0;
            table[match].used[i] = 0;
        }

        if (warmup_complete[cpu])
        L2_PF_DEBUG(printf("ST_miss cpu: %d cl_addr: %lx page: %lx block: %d
lru: %d\n", cpu, addr >> LOG2_BLOCK_SIZE, curr_page, curr_block,
table[match].lru)); L2_ST_miss[cpu]++; L2_ST_access[cpu]++;

        #ifdef L2_GHR_ON
        // Check GHR
        int ghr_max = 0, ghr_idx = -1, spec_block = 0, spec_sig = 0;
        for (int i=0; i<L2_GHR_TRACK; i++) {
            spec_block = L2_GHR[cpu][i].last_block + L2_GHR[cpu][i].oop_delta;
            if (spec_block >= 64)
                spec_block -= 64;
            else if (spec_block < 0)
                spec_block += 64;
            if ((spec_block == curr_block) && (ghr_max <=
L2_GHR[cpu][i].path_conf)) { ghr_max = L2_GHR[cpu][i].path_conf; ghr_idx = i;
                spec_sig = get_new_signature(L2_GHR[cpu][i].signature,
L2_GHR[cpu][i].oop_delta); if (warmup_complete[cpu]) L2_PF_DEBUG(printf("cpu: %d
OOP_match  L2_GHR[%d]  signature: %x  path_conf: %d  last_block: %d  oop_delta:
%d  spec_block: %d == curr_block: %d  spec_sig: %x\n", cpu, i,
L2_GHR[cpu][i].signature, L2_GHR[cpu][i].path_conf, L2_GHR[cpu][i].last_block,
                          L2_GHR[cpu][i].oop_delta, spec_block, curr_block,
spec_sig));
            }
            else {
                if (warmup_complete[cpu])
                L2_PF_DEBUG(printf("cpu: %d OOP_unmatch  L2_GHR[%d]  signature:
%x  path_conf: %d  last_block: %d  oop_delta: %d  spec_block: %d != curr_block:
%d  spec_sig: %x\n", cpu, i, L2_GHR[cpu][i].signature, L2_GHR[cpu][i].path_conf,
L2_GHR[cpu][i].last_block, L2_GHR[cpu][i].oop_delta, spec_block, curr_block,
spec_sig));
            }
        }

        if (ghr_idx >= 0) {
            // Speculatively update first page
            spec_sig = get_new_signature(L2_GHR[cpu][ghr_idx].signature,
L2_GHR[cpu][ghr_idx].oop_delta);

            hit = 1;
            table[match].signature = spec_sig;
            if (warmup_complete[cpu])
            L2_PF_DEBUG(printf("cpu: %d spec_update  page: %x  sig: %3x  delta:
%3d  curr_block: %2d  last_block[NA]: %2d\n", cpu, tag, spec_sig,
L2_GHR[cpu][ghr_idx].oop_delta, curr_block, L2_GHR[cpu][ghr_idx].last_block));
        }
        #endif
    }

    // Update LRU
    int position = table[match].lru;
    for (int i=0; i<L2_ST_WAY; i++) {
        if (table[i].lru < position)
            table[i].lru++;
    }
    table[match].lru = 0;

    if (hit)
        return match;
    else
        return -1;
}

int L2_ST_check(uint32_t cpu, uint64_t addr)
{
    uint64_t curr_page = addr >> LOG2_PAGE_SIZE;
    int tag = curr_page & 0xFFFF,
        match = -1,
        L2_ST_idx = curr_page % st_prime; //L2_ST_PRIME;

    SIGNATURE_TABLE *table = L2_ST[cpu][L2_ST_idx];

    for (match=0; match<L2_ST_WAY; match++) {
        if (table[match].valid && (table[match].tag == tag)) {
            if (warmup_complete[cpu])
            L2_PF_DEBUG(printf("ST_check found cpu: %d cl_addr: %lx page: %lx
block: %ld old_sig: %x last_block: %d\n", cpu, addr >> LOG2_BLOCK_SIZE,
curr_page, (addr >> LOG2_BLOCK_SIZE) & 0x3F, table[match].signature,
table[match].last_block)); return match;
        }
    }

    if (warmup_complete[cpu])
    L2_PF_DEBUG(printf("ST_check not found cpu: %d cl_addr: %lx page: %lx block:
%ld\n", cpu, addr >> LOG2_BLOCK_SIZE, curr_page, (addr >> LOG2_BLOCK_SIZE) &
0x3F)); return -1;
}

void L2_PT_update(uint32_t cpu, int signature, int delta)
{
    int L2_PT_idx = signature % pt_prime; //L2_PT_PRIME;
    PATTERN_TABLE *table = L2_PT[cpu][L2_PT_idx];

    // Update L2_PT
    // Update metadata
    table[0].c_sig++;

    if (table[0].c_sig == (CSIG_MAX))
    {
        table[0].c_sig = CSIG_MAX >> 1;
        for (int i = 0; i<L2_PT_WAY; i++)
            table[i].c_delta = table[i].c_delta >> 1;
        if (warmup_complete[cpu])
        L2_PF_DEBUG(printf("PT_sig: %4x cpu: %d c_sig saturated sig_total: %d =>
%d\n", L2_PT_idx, cpu, CSIG_MAX, table[0].c_sig));
    }

    int match;
    for (match=0; match<L2_PT_WAY; match++)
    {
        if (table[match].delta == delta) // Hit
        {
            table[match].c_delta++;

            if (warmup_complete[cpu])
            L2_PF_DEBUG(printf("PT_sig: %4x cpu: %d update_hit delta[%d]: %2d
(%d / %d)\n", signature, cpu, match, table[match].delta, table[match].c_delta,
table[0].c_sig)); L2_PT_hit[cpu]++; L2_PT_access[cpu]++; break;
        }
    }

    if (match == L2_PT_WAY)
    {
        for (match=0; match<L2_PT_WAY; match++)
        {
            if (table[match].delta == 0) // Invalid
            {
                // Update metadata
                table[match].delta = delta;
                table[match].c_delta = 0;

                if (warmup_complete[cpu])
                L2_PF_DEBUG(printf("PT_sig: %4x cpu: %d update_invalid
delta[%d]: %2d (%d / %d)\n", signature, cpu, match, table[match].delta,
table[match].c_delta, table[0].c_sig)); L2_PT_invalid[cpu]++;
L2_PT_access[cpu]++; break;
            }
        }
    }

    if (match == L2_PT_WAY) // Miss
    {
        // Search for the lowest counter
        int min_idx = -1;
        int min_val = CDELTA_MAX;
        for (match=0; match<L2_PT_WAY; match++)
        {
            if (table[match].c_delta < min_val)
            {
                min_idx = match;
                min_val = table[match].c_delta;
            }
        }
        match = min_idx;

        // Update metadata
        table[match].delta = delta;
        table[match].c_delta = 0;

        if (warmup_complete[cpu])
        L2_PF_DEBUG(printf("PT_sig: %4x cpu: %d update_miss delta[%d]: %2d (%d /
%d)\n", signature, cpu, match, table[match].delta, table[match].c_delta,
table[0].c_sig)); L2_PT_miss[cpu]++; L2_PT_access[cpu]++;
    }
}
*/

// TODO: from here
uint32_t CACHE::prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  // Check ST
  L2_ST_update(cpu, addr);

  // Check ST
  int l2_st_match = L2_ST_check(cpu, addr),
      l2_st_idx = (addr >> LOG2_PAGE_SIZE) % st_prime, // L2_ST_PRIME,
      curr_block = (addr >> LOG2_BLOCK_SIZE) & 0x3F;
  int pf_signature = 0, first_hit = 0;

  // Double check
  if (l2_st_match == -1)
    assert(0); // WE SHOULD NOT REACH HERE

  // Reset prefetch buffers
  MAX_CONF[cpu] = 99;
  num_pf[cpu] = 0;
  curr_conf[cpu] = 0;
  curr_delta[cpu] = 0;
  PF_inflight[cpu] = 0;
  out_of_page[cpu] = 0;
  not_enough_conf[cpu] = 0;
  for (int i = 0; i < L2C_MSHR_SIZE; i++) {
    pf_buffer[cpu][i].delta = 0;
    pf_buffer[cpu][i].signature = 0;
    pf_buffer[cpu][i].conf = 0;
    pf_buffer[cpu][i].depth = 0;
  }

  // Check bitmap
  // Mark bitmap (demand)
  if (L2_ST[cpu][l2_st_idx][l2_st_match].l2_pf[curr_block] && (L2_ST[cpu][l2_st_idx][l2_st_match].used[curr_block] == 0)) {
    spp_pf_useful[cpu]++;
    useful_depth[cpu][L2_ST[cpu][l2_st_idx][l2_st_match].depth[curr_block]]++;

    /*
    // Notify sampler
    int set = mlc_get_set(addr);
    int way = mlc_get_way(cpu, addr, set);
    notify_sampler(cpu, addr, mlc_cache[cpu][set][way].dirty, 3,
    L2_ST[cpu][l2_st_idx][l2_st_match].signature);
    */
  }
  L2_ST[cpu][l2_st_idx][l2_st_match].used[curr_block] = 1;

  // Dynamically update MAX_CONF (measured by ST)
  if (spp_pf_issued[cpu])
    MAX_CONF[cpu] = (100 * spp_pf_useful[cpu]) / spp_pf_issued[cpu];

  if (MAX_CONF[cpu] >= 99)
    MAX_CONF[cpu] = 99;

  // Search for prefetch candidate when we have a non-zero signature
  pf_signature = L2_ST[cpu][l2_st_idx][l2_st_match].signature;
  first_hit = L2_ST[cpu][l2_st_idx][l2_st_match].first_hit;
  if (pf_signature && (first_hit == 0))
    PF_check(cpu, pf_signature, curr_block);

  // Request prefetch
  uint64_t pf_addr = 0;
  int pf_block = 0;
  if (warmup_complete[cpu])
    L2_PF_DEBUG(printf("pf_delta: "));
  for (int i = 0; i < num_pf[cpu]; i++) {
    if (pf_buffer[cpu][i].delta == 0) {
      printf("pf_delta[%d][%d]: %d  num_pf_delta: %d\n", cpu, i, pf_buffer[cpu][i].delta, num_pf[cpu]);
      assert(0);
    } else {
      if (warmup_complete[cpu])
        L2_PF_DEBUG(printf("%d ", pf_buffer[cpu][i].delta));
    }
  }
  if (warmup_complete[cpu])
    L2_PF_DEBUG(printf("\n"));

  for (int i = 0; i < num_pf[cpu]; i++) {
    if (pf_buffer[cpu][i].delta == 0) {
      printf("pf_delta[%d][%d]: %d  num_pf_delta: %d\n", cpu, i, pf_buffer[cpu][i].delta, num_pf[cpu]);
      assert(0);
    } else {
      // Actual prefetch request, calculate prefetch address
      pf_addr = ((addr >> LOG2_BLOCK_SIZE) + pf_buffer[cpu][i].delta) << LOG2_BLOCK_SIZE;
      pf_block = (pf_addr >> LOG2_BLOCK_SIZE) & 0x3F;

      // Check bitmap
      int l2_pf = L2_ST[cpu][l2_st_idx][l2_st_match].l2_pf[pf_block], l2_demand = L2_ST[cpu][l2_st_idx][l2_st_match].used[pf_block];

      // if (bitmap_check)
      if (l2_pf || l2_demand) {
        if (warmup_complete[cpu])
          L2_PF_DEBUG(printf("Prefetch is filtered  key: %lx\n", pf_addr >> LOG2_BLOCK_SIZE));
      } else {
        if (pf_buffer[cpu][i].conf >= FILL_THRESHOLD) { // Prefetch to the L2
          if (prefetch_line(ip, addr, pf_addr, FILL_L2, 0)) {
            PF_inflight[cpu]++;
            if (warmup_complete[cpu])
              L2_PF_DEBUG(printf("L2_PREFETCH  cpu: %d base_cl: %lx pf_cl: %lx delta: "
                                 "%d d_sig: %x pf_sig: %x depth: %d conf: %d\n",
                                 cpu, addr >> LOG2_BLOCK_SIZE, pf_addr >> LOG2_BLOCK_SIZE, pf_buffer[cpu][i].delta, pf_buffer[cpu][i].signature,
                                 pf_buffer[cpu][i].conf, pf_buffer[cpu][i].depth, pf_buffer[cpu][i].conf));

            // Mark bitmap (prefetch)
            L2_ST[cpu][l2_st_idx][l2_st_match].l2_pf[pf_block] = 1;
            L2_ST[cpu][l2_st_idx][l2_st_match].delta[pf_block] = ((int64_t)pf_addr >> LOG2_BLOCK_SIZE) - ((int64_t)addr >> LOG2_BLOCK_SIZE);
            L2_ST[cpu][l2_st_idx][l2_st_match].depth[pf_block] = PF_inflight[cpu];

            spp_pf_issued[cpu]++;
            if (spp_pf_issued[cpu] > GC_MAX) {
              spp_pf_issued[cpu] /= 2;
              spp_pf_useful[cpu] /= 2;
            }
          }
        } else if (pf_buffer[cpu][i].conf >= PF_THRESHOLD) { // Prefetch to the LLC
          if (prefetch_line(ip, addr, pf_addr, FILL_LLC, 0)) {
            PF_inflight[cpu]++;
            if (warmup_complete[cpu])
              L2_PF_DEBUG(printf("LLC_PREFETCH cpu: %d base_cl: %lx pf_cl: %lx delta: "
                                 "%d d_sig: %x pf_sig: %x depth: %d conf: %d\n",
                                 cpu, addr >> LOG2_BLOCK_SIZE, pf_addr >> LOG2_BLOCK_SIZE, pf_buffer[cpu][i].delta, pf_buffer[cpu][i].signature,
                                 pf_buffer[cpu][i].conf, pf_buffer[cpu][i].depth, pf_buffer[cpu][i].conf));
          }
        }
      }
    }
  }

  if (warmup_complete[cpu])
    L2_PF_DEBUG(printf("\n"));

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  // L2 FILL
  uint64_t evicted_cl = evicted_addr >> LOG2_BLOCK_SIZE;

  if (evicted_cl) {
    // Clear bitmap
    int l2_st_match = L2_ST_check(cpu, evicted_addr),
        l2_st_idx = (evicted_addr >> LOG2_PAGE_SIZE) % st_prime, // L2_ST_PRIME,
        evicted_block = evicted_cl & 0x3F;
    SIGNATURE_TABLE* table = L2_ST[cpu][l2_st_idx];

    if (l2_st_match >= 0) {
      int evicted_depth = table[l2_st_match].depth[evicted_block];

      if (table[l2_st_match].l2_pf[evicted_block]) {
        if (table[l2_st_match].used[evicted_block] == 0) {
          spp_pf_useless[cpu]++;
          useless_depth[cpu][evicted_depth]++;

          L2_PF_DEBUG(if (warmup_complete[cpu]) {
            cout << "Useless pf_addr: " << hex << evicted_cl << dec << " delta: " << table[l2_st_match].delta[evicted_block];
            cout << " depth: " << table[l2_st_match].depth[evicted_block] << endl;
          });

          // Notify sampler
          // notify_sampler(cpu, evicted_addr, evicted_dirty, 0);
        }
        // else
        // notify_sampler(cpu, evicted_addr, evicted_dirty, 1);
      }
      table[l2_st_match].l2_pf[evicted_block] = 0;
      table[l2_st_match].used[evicted_block] = 0;
    }
  }

  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats()
{
  std::cout << std::endl << NAME << " Signature Path Prefetcher final stats" << std::endl;

  /*
  int temp1 = 0, temp2 = 0;
  for (int i=0; i<L2C_MSHR_SIZE; i++)
  {
      temp1 += useful_depth[cpu][i];
      temp2 += useless_depth[cpu][i];
  }
  for (int i=0; i<L2C_MSHR_SIZE; i++)
      printf("mlc_useful_depth %2d %5.1f%% %10d  mlc_useless_depth %2d %5.1f%%
  %10d\n", i, (100.0*useful_depth[cpu][i])/temp1, useful_depth[cpu][i], i,
  (100.0*useless_depth[cpu][i])/temp2, useless_depth[cpu][i]);
  */
}
