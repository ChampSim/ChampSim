/*
//#include "cache.h"
#include "kpcp.h"

SIGNATURE_TABLE L2_ST[NUM_CPUS][L2_ST_SET][L2_ST_WAY];
PATTERN_TABLE L2_PT[NUM_CPUS][L2_PT_SET][L2_PT_WAY];
GLOBAL_HISTORY_REGISTER L2_GHR[NUM_CPUS][L2_GHR_TRACK];

int L2_ST_access[NUM_CPUS], L2_ST_hit[NUM_CPUS], L2_ST_invalid[NUM_CPUS],
L2_ST_miss[NUM_CPUS]; int L2_PT_access[NUM_CPUS], L2_PT_hit[NUM_CPUS],
L2_PT_invalid[NUM_CPUS], L2_PT_miss[NUM_CPUS]; int
l2_sig_dist[NUM_CPUS][1<<SIG_LENGTH];

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
        L2_ST_idx = curr_page % L2_ST_PRIME,
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
        L2_ST_idx = curr_page % L2_ST_PRIME;

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
    int L2_PT_idx = signature % L2_PT_PRIME;
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

// TODO: this functino should be moved to the replacement policy file
// Check sampler
// void notify_sampler(uint32_t cpu, int64_t address, int dirty, int useful)
//{
/*
int set = llc_get_set(address);
int s_idx = is_it_sampled(set);

if (s_idx == -1)
    return;

SAMPLER_T *s_set = sampler[s_idx];
int tag = (int) address / (64*LLC_SETS);
int match = -1;

// Check hit
for (match=0; match<SAMPLER_WAY; match++)
{
    if (s_set[match].valid && (s_set[match].tag == tag))
    {
        if (s_set[match].l2pf)
        {
            if (useful)
            {
                if (conf_counter[cpu] < MAX_CC)
                    conf_counter[cpu]++;

                if (conf_counter[cpu] == MAX_CC)
                {
                    if (dynamic_fill_thrs[cpu] > 0)
                    {
                        dynamic_fill_thrs[cpu]--;
                        fill_down++;
                        conf_level[dynamic_fill_thrs[cpu]]++;

                        printf("FILL_THRESHOLD goes down %d => %d at cycle:
%ld\n", dynamic_fill_thrs[cpu]+1, dynamic_fill_thrs[cpu],
ooo_cpu[cpu].current_cycle);
                    }

                    conf_counter[cpu] = 0;
                }

                l2pf_was_useful++;
            }
            else
            {
                if (conf_counter[cpu] > 0)
                    conf_counter[cpu]--;

                l2pf_was_useless++;
            }

            l2pf_match++;
        }

        break;
    }
}
l2pf_signal++;

return;
*/
//}
