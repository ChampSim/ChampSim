////////////////////////////////////////////////////////////////////////
//
//  Code submitted for the First Instruction Prefetching Championship
//
//  Authors: Alberto Ros (aros@ditec.um.es)
//           Alexandra Jimborean (alexandra.jimborean@um.es)
//
//  Paper #30: The Entangling Instruction Prefetcher
//
////////////////////////////////////////////////////////////////////////

#include "ooo_cpu.h"

#define L1I_PQ_SIZE 32
#define L1I_MSHR_SIZE 8
#define L1I_SET 64
#define L1I_WAY 8

#define DEBUG(x) 

// To access cpu in my functions
uint32_t l1i_cpu_id;

uint64_t l1i_last_basic_block;
uint32_t l1i_consecutive_count;
uint32_t l1i_basic_block_merge_diff;

// LINE AND MERGE BASIC BLOCK SIZE

#define L1I_MERGE_BBSIZE_BITS 7
#define L1I_MERGE_BBSIZE_MAX_VALUE ((1 << L1I_MERGE_BBSIZE_BITS) - 1)

// TIME AND OVERFLOWS

#define L1I_TIME_DIFF_BITS 20
#define L1I_TIME_DIFF_OVERFLOW ((uint64_t)1 << L1I_TIME_DIFF_BITS)
#define L1I_TIME_DIFF_MASK (L1I_TIME_DIFF_OVERFLOW - 1)

#define L1I_TIME_BITS 12
#define L1I_TIME_OVERFLOW ((uint64_t)1 << L1I_TIME_BITS)
#define L1I_TIME_MASK (L1I_TIME_OVERFLOW - 1)

uint64_t l1i_get_latency(uint64_t cycle, uint64_t cycle_prev)
{
  uint64_t cycle_masked = cycle & L1I_TIME_MASK;
  uint64_t cycle_prev_masked = cycle_prev & L1I_TIME_MASK;
  if (cycle_prev_masked > cycle_masked) {
    return (cycle_masked + L1I_TIME_OVERFLOW) - cycle_prev_masked;
  }
  return cycle_masked - cycle_prev_masked;
}

// ENTANGLED COMPRESSION FORMAT

#define L1I_ENTANGLED_MAX_FORMATS 7

// HISTORY TABLE

#define L1I_HIST_TABLE_ENTRIES 1072
#define L1I_HIST_TABLE_MASK (L1I_HIST_TABLE_ENTRIES - 1)
#define L1I_BB_MERGE_ENTRIES 4
#define L1I_HIST_TAG_BITS 58
#define L1I_HIST_TAG_MASK (((uint64_t)1 << L1I_HIST_TAG_BITS) - 1)

typedef struct __l1i_hist_entry {
  uint64_t tag;       // L1I_HIST_TAG_BITS bits
  uint64_t time_diff; // L1I_TIME_DIFF_BITS bits
  uint32_t bb_size;   // L1I_MERGE_BBSIZE_BITS bits
} l1i_hist_entry;

l1i_hist_entry l1i_hist_table[NUM_CPUS][L1I_HIST_TABLE_ENTRIES];
uint64_t l1i_hist_table_head[NUM_CPUS];      // log_2 (L1I_HIST_TABLE_ENTRIES)
uint64_t l1i_hist_table_head_time[NUM_CPUS]; // 64 bits

void l1i_init_hist_table()
{
  l1i_hist_table_head[l1i_cpu_id] = 0;
  l1i_hist_table_head_time[l1i_cpu_id] = current_core_cycle[l1i_cpu_id];
  for (uint32_t i = 0; i < L1I_HIST_TABLE_ENTRIES; i++) {
    l1i_hist_table[l1i_cpu_id][i].tag = 0;
    l1i_hist_table[l1i_cpu_id][i].time_diff = 0;
    l1i_hist_table[l1i_cpu_id][i].bb_size = 0;
  }
}

uint64_t l1i_find_hist_entry(uint64_t line_addr)
{
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK;
  for (uint32_t count = 0, i = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES; count < L1I_HIST_TABLE_ENTRIES;
       count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    if (l1i_hist_table[l1i_cpu_id][i].tag == tag)
      return i;
  }
  return L1I_HIST_TABLE_ENTRIES;
}

// It can have duplicated entries if the line was evicted in between
void l1i_add_hist_table(uint64_t line_addr)
{
  // Insert empty addresses in hist not to have timediff overflows
  while (current_core_cycle[l1i_cpu_id] - l1i_hist_table_head_time[l1i_cpu_id] >= L1I_TIME_DIFF_OVERFLOW) {
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].tag = 0;
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].time_diff = L1I_TIME_DIFF_MASK;
    l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].bb_size = 0;
    l1i_hist_table_head[l1i_cpu_id] = (l1i_hist_table_head[l1i_cpu_id] + 1) % L1I_HIST_TABLE_ENTRIES;
    l1i_hist_table_head_time[l1i_cpu_id] += L1I_TIME_DIFF_MASK;
  }

  // Allocate a new entry (evict old one if necessary)
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].tag = line_addr & L1I_HIST_TAG_MASK;
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].time_diff =
      (current_core_cycle[l1i_cpu_id] - l1i_hist_table_head_time[l1i_cpu_id]) & L1I_TIME_DIFF_MASK;
  l1i_hist_table[l1i_cpu_id][l1i_hist_table_head[l1i_cpu_id]].bb_size = 0;
  l1i_hist_table_head[l1i_cpu_id] = (l1i_hist_table_head[l1i_cpu_id] + 1) % L1I_HIST_TABLE_ENTRIES;
  l1i_hist_table_head_time[l1i_cpu_id] = current_core_cycle[l1i_cpu_id];
}

void l1i_add_bb_size_hist_table(uint64_t line_addr, uint32_t bb_size)
{
  uint64_t index = l1i_find_hist_entry(line_addr);
  l1i_hist_table[l1i_cpu_id][index].bb_size = bb_size & L1I_MERGE_BBSIZE_MAX_VALUE;
}

// compute delta
uint32_t l1i_find_bb_merge_hist_table(uint64_t line_addr)
{
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK;
  for (uint32_t count = 0, i = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES; count < L1I_HIST_TABLE_ENTRIES;
       count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    if (count >= L1I_BB_MERGE_ENTRIES) {
      return 0;
    }
    if (tag > l1i_hist_table[l1i_cpu_id][i].tag && (tag - l1i_hist_table[l1i_cpu_id][i].tag) <= l1i_hist_table[l1i_cpu_id][i].bb_size) {
      //&& (tag - l1i_hist_table[l1i_cpu_id][i].tag) == l1i_hist_table[l1i_cpu_id][i].bb_size) {
      return tag - l1i_hist_table[l1i_cpu_id][i].tag;
    }
  }
  assert(false);
}

// return bere (best request -- entangled address)
uint64_t l1i_get_bere_hist_table(uint64_t line_addr, uint64_t latency, uint32_t skip = 0)
{
  uint64_t tag = line_addr & L1I_HIST_TAG_MASK;
  assert(tag);
  uint32_t first = (l1i_hist_table_head[l1i_cpu_id] + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES;
  uint64_t time_i = l1i_hist_table_head_time[l1i_cpu_id];
  uint64_t req_time = 0;
  uint32_t num_skipped = 0;
  for (uint32_t count = 0, i = first; count < L1I_HIST_TABLE_ENTRIES; count++, i = (i + L1I_HIST_TABLE_MASK) % L1I_HIST_TABLE_ENTRIES) {
    // Against the time overflow
    if (req_time == 0 && l1i_hist_table[l1i_cpu_id][i].tag == tag && time_i + latency >= current_core_cycle[l1i_cpu_id]) { // Its me (miss or late prefetcher)
      req_time = time_i;
    } else if (req_time) { // Not me (check only older than me)
      if (l1i_hist_table[l1i_cpu_id][i].tag == tag) {
        return 0; // Second time it appeared (it was evicted in between) or many for the same set. No entangle
      }
      if (time_i + latency <= req_time && l1i_hist_table[l1i_cpu_id][i].tag) {
        if (skip == num_skipped) {
          return l1i_hist_table[l1i_cpu_id][i].tag;
        } else {
          num_skipped++;
        }
      }
    }
    time_i -= l1i_hist_table[l1i_cpu_id][i].time_diff;
  }
  return 0;
}

// TIMING TABLES

#define L1I_SET_BITS 6
#define L1I_TIMING_MSHR_SIZE (L1I_PQ_SIZE + L1I_MSHR_SIZE + 2)
#define L1I_TIMING_MSHR_TAG_BITS 42
#define L1I_TIMING_MSHR_TAG_MASK (((uint64_t)1 << L1I_HIST_TAG_BITS) - 1)
#define L1I_TIMING_CACHE_TAG_BITS (L1I_TIMING_MSHR_TAG_BITS - L1I_SET_BITS)
#define L1I_TIMING_CACHE_TAG_MASK (((uint64_t)1 << L1I_HIST_TAG_BITS) - 1)

// We do not have access to the MSHR, so we aproximate it using this structure
typedef struct __l1i_timing_mshr_entry {
  bool valid;              // 1 bit
  uint64_t tag;            // L1I_TIMING_MSHR_TAG_BITS bits
  uint64_t bere_line_addr; // 58 bits
  uint64_t timestamp;      // L1I_TIME_BITS bits // time when issued
  bool accessed;           // 1 bit
} l1i_timing_mshr_entry;

// We do not have access to the cache, so we aproximate it using this structure
typedef struct __l1i_timing_cache_entry {
  bool valid;              // 1 bit
  uint64_t tag;            // L1I_TIMING_CACHE_TAG_BITS bits
  uint64_t bere_line_addr; // 58 bits
  bool accessed;           // 1 bit
} l1i_timing_cache_entry;

l1i_timing_mshr_entry l1i_timing_mshr_table[NUM_CPUS][L1I_TIMING_MSHR_SIZE];
l1i_timing_cache_entry l1i_timing_cache_table[NUM_CPUS][L1I_SET][L1I_WAY];

void l1i_init_timing_tables()
{
  for (uint32_t i = 0; i < L1I_TIMING_MSHR_SIZE; i++) {
    l1i_timing_mshr_table[l1i_cpu_id][i].valid = 0;
  }
  for (uint32_t i = 0; i < L1I_SET; i++) {
    for (uint32_t j = 0; j < L1I_WAY; j++) {
      l1i_timing_cache_table[l1i_cpu_id][i][j].valid = 0;
    }
  }
}

uint64_t l1i_find_timing_mshr_entry(uint64_t line_addr)
{
  for (uint32_t i = 0; i < L1I_TIMING_MSHR_SIZE; i++) {
    if (l1i_timing_mshr_table[l1i_cpu_id][i].tag == (line_addr & L1I_TIMING_MSHR_TAG_MASK) && l1i_timing_mshr_table[l1i_cpu_id][i].valid)
      return i;
  }
  return L1I_TIMING_MSHR_SIZE;
}

uint64_t l1i_find_timing_cache_entry(uint64_t line_addr)
{
  uint64_t i = line_addr % L1I_SET;
  for (uint32_t j = 0; j < L1I_WAY; j++) {
    if (l1i_timing_cache_table[l1i_cpu_id][i][j].tag == ((line_addr >> L1I_SET_BITS) & L1I_TIMING_CACHE_TAG_MASK)
        && l1i_timing_cache_table[l1i_cpu_id][i][j].valid)
      return j;
  }
  return L1I_WAY;
}

uint32_t l1i_get_invalid_timing_mshr_entry()
{
  for (uint32_t i = 0; i < L1I_TIMING_MSHR_SIZE; i++) {
    if (!l1i_timing_mshr_table[l1i_cpu_id][i].valid)
      return i;
  }
  assert(false); // It must return a free entry
  return L1I_TIMING_MSHR_SIZE;
}

uint32_t l1i_get_invalid_timing_cache_entry(uint64_t line_addr)
{
  uint32_t i = line_addr % L1I_SET;
  for (uint32_t j = 0; j < L1I_WAY; j++) {
    if (!l1i_timing_cache_table[l1i_cpu_id][i][j].valid)
      return j;
  }
  assert(false); // It must return a free entry
  return L1I_WAY;
}

void l1i_add_timing_entry(uint64_t line_addr, uint64_t bere_line_addr)
{
  // First find for coalescing
  if (l1i_find_timing_mshr_entry(line_addr) < L1I_TIMING_MSHR_SIZE)
    return;
  if (l1i_find_timing_cache_entry(line_addr) < L1I_WAY)
    return;

  uint32_t i = l1i_get_invalid_timing_mshr_entry();
  l1i_timing_mshr_table[l1i_cpu_id][i].valid = true;
  l1i_timing_mshr_table[l1i_cpu_id][i].tag = line_addr & L1I_TIMING_MSHR_TAG_MASK;
  l1i_timing_mshr_table[l1i_cpu_id][i].bere_line_addr = bere_line_addr;
  l1i_timing_mshr_table[l1i_cpu_id][i].timestamp = current_core_cycle[l1i_cpu_id] & L1I_TIME_MASK;
  l1i_timing_mshr_table[l1i_cpu_id][i].accessed = false;
  DEBUG(cout << "Add Entry: " << i << endl;)
}

void l1i_invalid_timing_mshr_entry(uint64_t line_addr)
{
  uint32_t index = l1i_find_timing_mshr_entry(line_addr);
  assert(index < L1I_TIMING_MSHR_SIZE);
  l1i_timing_mshr_table[l1i_cpu_id][index].valid = false;
  DEBUG(cout << "Invalid Entry: " << index << endl;)
}

void l1i_move_timing_entry(uint64_t line_addr)
{
  uint32_t index_mshr = l1i_find_timing_mshr_entry(line_addr);
  if (index_mshr == L1I_TIMING_MSHR_SIZE) {
    uint32_t set = line_addr % L1I_SET;
    uint32_t index_cache = l1i_get_invalid_timing_cache_entry(line_addr);
    l1i_timing_cache_table[l1i_cpu_id][set][index_cache].valid = true;
    l1i_timing_cache_table[l1i_cpu_id][set][index_cache].tag = (line_addr >> L1I_SET_BITS) & L1I_TIMING_CACHE_TAG_MASK;
    l1i_timing_cache_table[l1i_cpu_id][set][index_cache].accessed = true;
    return;
  }
  uint64_t set = line_addr % L1I_SET;
  uint64_t index_cache = l1i_get_invalid_timing_cache_entry(line_addr);
  l1i_timing_cache_table[l1i_cpu_id][set][index_cache].valid = true;
  l1i_timing_cache_table[l1i_cpu_id][set][index_cache].tag = (line_addr >> L1I_SET_BITS) & L1I_TIMING_CACHE_TAG_MASK;
  l1i_timing_cache_table[l1i_cpu_id][set][index_cache].bere_line_addr = l1i_timing_mshr_table[l1i_cpu_id][index_mshr].bere_line_addr;
  l1i_timing_cache_table[l1i_cpu_id][set][index_cache].accessed = l1i_timing_mshr_table[l1i_cpu_id][index_mshr].accessed;
  l1i_invalid_timing_mshr_entry(line_addr);
}

// returns if accessed
bool l1i_invalid_timing_cache_entry(uint64_t line_addr, uint64_t& bere_line_addr)
{
  uint32_t set = line_addr % L1I_SET;
  uint32_t way = l1i_find_timing_cache_entry(line_addr);
  assert(way < L1I_WAY);
  l1i_timing_cache_table[l1i_cpu_id][set][way].valid = false;
  bere_line_addr = l1i_timing_cache_table[l1i_cpu_id][set][way].bere_line_addr;
  return l1i_timing_cache_table[l1i_cpu_id][set][way].accessed;
}

void l1i_access_timing_entry(uint64_t line_addr)
{
  uint32_t index = l1i_find_timing_mshr_entry(line_addr);
  if (index < L1I_TIMING_MSHR_SIZE) {
    if (!l1i_timing_mshr_table[l1i_cpu_id][index].accessed) {
      l1i_timing_mshr_table[l1i_cpu_id][index].accessed = true;
    }
    return;
  }
  uint32_t set = line_addr % L1I_SET;
  uint32_t way = l1i_find_timing_cache_entry(line_addr);
  if (way < L1I_WAY) {
    l1i_timing_cache_table[l1i_cpu_id][set][way].accessed = true;
  }
}

bool l1i_is_accessed_timing_entry(uint64_t line_addr)
{
  uint32_t index = l1i_find_timing_mshr_entry(line_addr);
  if (index < L1I_TIMING_MSHR_SIZE) {
    return l1i_timing_mshr_table[l1i_cpu_id][index].accessed;
  }
  uint32_t set = line_addr % L1I_SET;
  uint32_t way = l1i_find_timing_cache_entry(line_addr);
  if (way < L1I_WAY) {
    return l1i_timing_cache_table[l1i_cpu_id][set][way].accessed;
  }
  return false;
}

bool l1i_completed_request(uint64_t line_addr) { return l1i_find_timing_cache_entry(line_addr) < L1I_WAY; }

bool l1i_ongoing_request(uint64_t line_addr) { return l1i_find_timing_mshr_entry(line_addr) < L1I_TIMING_MSHR_SIZE; }

bool l1i_ongoing_accessed_request(uint64_t line_addr)
{
  uint32_t index = l1i_find_timing_mshr_entry(line_addr);
  if (index == L1I_TIMING_MSHR_SIZE)
    return false;
  return l1i_timing_mshr_table[l1i_cpu_id][index].accessed;
}

uint64_t l1i_get_latency_timing_mshr(uint64_t line_addr)
{
  uint32_t index = l1i_find_timing_mshr_entry(line_addr);
  if (index == L1I_TIMING_MSHR_SIZE)
    return 0;
  if (!l1i_timing_mshr_table[l1i_cpu_id][index].accessed)
    return 0;
  return l1i_get_latency(current_core_cycle[l1i_cpu_id], l1i_timing_mshr_table[l1i_cpu_id][index].timestamp);
}

// RECORD ENTANGLED TABLE

uint32_t L1I_ENTANGLED_FORMATS[L1I_ENTANGLED_MAX_FORMATS] = {58, 28, 18, 13, 10, 8, 6};
#define L1I_ENTANGLED_NUM_FORMATS 6

uint32_t l1i_get_format_entangled(uint64_t line_addr, uint64_t entangled_addr)
{
  for (uint32_t i = L1I_ENTANGLED_NUM_FORMATS; i != 0; i--) {
    if ((line_addr >> L1I_ENTANGLED_FORMATS[i - 1]) == (entangled_addr >> L1I_ENTANGLED_FORMATS[i - 1])) {
      return i;
    }
  }
  assert(false);
}

uint64_t l1i_extend_format_entangled(uint64_t line_addr, uint64_t entangled_addr, uint32_t format)
{
  return ((line_addr >> L1I_ENTANGLED_FORMATS[format - 1]) << L1I_ENTANGLED_FORMATS[format - 1])
         | (entangled_addr & (((uint64_t)1 << L1I_ENTANGLED_FORMATS[format - 1]) - 1));
}

uint64_t l1i_compress_format_entangled(uint64_t entangled_addr, uint32_t format)
{
  return entangled_addr & (((uint64_t)1 << L1I_ENTANGLED_FORMATS[format - 1]) - 1);
}

#define L1I_ENTANGLED_TABLE_INDEX_BITS 8
#define L1I_ENTANGLED_TABLE_SETS (1 << L1I_ENTANGLED_TABLE_INDEX_BITS)
#define L1I_ENTANGLED_TABLE_WAYS 34
#define L1I_MAX_ENTANGLED_PER_LINE L1I_ENTANGLED_NUM_FORMATS
#define L1I_TAG_BITS (42 - L1I_ENTANGLED_TABLE_INDEX_BITS)
#define L1I_TAG_MASK (((uint64_t)1 << L1I_TAG_BITS) - 1)
#define L1I_CONFIDENCE_COUNTER_BITS 2
#define L1I_CONFIDENCE_COUNTER_MAX_VALUE ((1 << L1I_CONFIDENCE_COUNTER_BITS) - 1)
#define L1I_CONFIDENCE_COUNTER_THRESHOLD 1

#define L1I_TRIES_AVAIL_ENTANGLED 6
#define L1I_TRIES_AVAIL_ENTANGLED_NOT_PRESENT 2

typedef struct __l1i_entangled_entry {
  uint64_t tag;                                        // L1I_TAG_BITS bits
  uint32_t format;                                     // 3 bits
  uint64_t entangled_addr[L1I_MAX_ENTANGLED_PER_LINE]; // keep just diff
  uint32_t entangled_conf[L1I_MAX_ENTANGLED_PER_LINE]; // L1I_CONFIDENCE_COUNTER_BITS bits
  uint32_t bb_size;                                    // L1I_MERGE_BBSIZE_BITS bits
} l1i_entangled_entry;

l1i_entangled_entry l1i_entangled_table[NUM_CPUS][L1I_ENTANGLED_TABLE_SETS][L1I_ENTANGLED_TABLE_WAYS];
uint32_t l1i_entangled_fifo[NUM_CPUS][L1I_ENTANGLED_TABLE_SETS]; // log2(L1I_ENTANGLED_TABLE_WAYS) * L1I_ENTANGLED_TABLE_SETS bits

void l1i_init_entangled_table()
{
  for (uint32_t i = 0; i < L1I_ENTANGLED_TABLE_SETS; i++) {
    for (uint32_t j = 0; j < L1I_ENTANGLED_TABLE_WAYS; j++) {
      l1i_entangled_table[l1i_cpu_id][i][j].tag = 0;
      l1i_entangled_table[l1i_cpu_id][i][j].format = 1;
      for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
        l1i_entangled_table[l1i_cpu_id][i][j].entangled_addr[k] = 0;
        l1i_entangled_table[l1i_cpu_id][i][j].entangled_conf[k] = 0;
      }
      l1i_entangled_table[l1i_cpu_id][i][j].bb_size = 0;
    }
    l1i_entangled_fifo[l1i_cpu_id][i] = 0;
  }
}

uint32_t l1i_get_way_entangled_table(uint64_t line_addr)
{
  uint64_t tag = (line_addr >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK;
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  for (int i = 0; i < L1I_ENTANGLED_TABLE_WAYS; i++) {
    if (l1i_entangled_table[l1i_cpu_id][set][i].tag == tag) { // Found
      return i;
    }
  }
  return L1I_ENTANGLED_TABLE_WAYS;
}

void l1i_add_entangled_table(uint64_t line_addr, uint64_t entangled_addr)
{
  uint64_t tag = (line_addr >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK;
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS) {
    way = l1i_entangled_fifo[l1i_cpu_id][set];
    l1i_entangled_table[l1i_cpu_id][set][way].tag = tag;
    l1i_entangled_table[l1i_cpu_id][set][way].format = 1;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = 0;
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = 0;
    }
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = 0;
    l1i_entangled_fifo[l1i_cpu_id][set] = (l1i_entangled_fifo[l1i_cpu_id][set] + 1) % L1I_ENTANGLED_TABLE_WAYS;
  }
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
        && l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format)
               == entangled_addr) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = L1I_CONFIDENCE_COUNTER_MAX_VALUE;
      return;
    }
  }

  // Adding a new entangled
  uint32_t format_new = l1i_get_format_entangled(line_addr, entangled_addr);

  // Check for evictions
  while (true) {
    uint32_t min_format = format_new;
    uint32_t num_valid = 1;
    uint32_t min_value = L1I_CONFIDENCE_COUNTER_MAX_VALUE + 1;
    uint32_t min_pos = 0;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
        num_valid++;
        uint32_t format_k =
            l1i_get_format_entangled(line_addr, l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k],
                                                                            l1i_entangled_table[l1i_cpu_id][set][way].format));
        if (format_k < min_format) {
          min_format = format_k;
        }
        if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < min_value) {
          min_value = l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k];
          min_pos = k;
        }
      }
    }
    if (num_valid > min_format) { // Eviction is necessary. We chose the lower confidence one
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[min_pos] = 0;
    } else {
      // Reformat
      for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
        if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
          l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] =
              l1i_compress_format_entangled(l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k],
                                                                        l1i_entangled_table[l1i_cpu_id][set][way].format),
                                            min_format);
        }
      }
      l1i_entangled_table[l1i_cpu_id][set][way].format = min_format;
      break;
    }
  }
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] =
          l1i_compress_format_entangled(entangled_addr, l1i_entangled_table[l1i_cpu_id][set][way].format);
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = L1I_CONFIDENCE_COUNTER_MAX_VALUE;
      return;
    }
  }
}

bool l1i_avail_entangled_table(uint64_t line_addr, uint64_t entangled_addr, bool insert_not_present)
{
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS)
    return insert_not_present;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
        && l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k], l1i_entangled_table[l1i_cpu_id][set][way].format)
               == entangled_addr) {
      return true;
    }
  }
  // Check for availability
  uint32_t min_format = l1i_get_format_entangled(line_addr, entangled_addr);
  uint32_t num_valid = 1;
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      num_valid++;
      uint32_t format_k =
          l1i_get_format_entangled(line_addr, l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k],
                                                                          l1i_entangled_table[l1i_cpu_id][set][way].format));
      if (format_k < min_format) {
        min_format = format_k;
      }
    }
  }
  if (num_valid > min_format) { // Eviction is necessary
    return false;
  } else {
    return true;
  }
}

void l1i_add_bbsize_table(uint64_t line_addr, uint32_t bb_size)
{
  uint64_t tag = (line_addr >> L1I_ENTANGLED_TABLE_INDEX_BITS) & L1I_TAG_MASK;
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way == L1I_ENTANGLED_TABLE_WAYS) {
    way = l1i_entangled_fifo[l1i_cpu_id][set];
    l1i_entangled_table[l1i_cpu_id][set][way].tag = tag;
    l1i_entangled_table[l1i_cpu_id][set][way].format = 1;
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k] = 0;
      l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] = 0;
    }
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = 0;
    l1i_entangled_fifo[l1i_cpu_id][set] = (l1i_entangled_fifo[l1i_cpu_id][set] + 1) % L1I_ENTANGLED_TABLE_WAYS;
  }
  if (bb_size > l1i_entangled_table[l1i_cpu_id][set][way].bb_size) {
    l1i_entangled_table[l1i_cpu_id][set][way].bb_size = bb_size & L1I_MERGE_BBSIZE_MAX_VALUE;
  }
}

uint64_t l1i_get_entangled_addr_entangled_table(uint64_t line_addr, uint32_t index_k)
{
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[index_k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD) {
      return l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[index_k],
                                         l1i_entangled_table[l1i_cpu_id][set][way].format);
    }
  }
  return 0;
}

uint32_t l1i_get_bbsize_entangled_table(uint64_t line_addr)
{
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    return l1i_entangled_table[l1i_cpu_id][set][way].bb_size;
  }
  return 0;
}

void l1i_update_confidence_entangled_table(uint64_t line_addr, uint64_t entangled_addr, bool accessed)
{
  uint32_t set = line_addr % L1I_ENTANGLED_TABLE_SETS;
  uint32_t way = l1i_get_way_entangled_table(line_addr);
  if (way < L1I_ENTANGLED_TABLE_WAYS) {
    for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
      if (l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] >= L1I_CONFIDENCE_COUNTER_THRESHOLD
          && l1i_extend_format_entangled(line_addr, l1i_entangled_table[l1i_cpu_id][set][way].entangled_addr[k],
                                         l1i_entangled_table[l1i_cpu_id][set][way].format)
                 == entangled_addr) {
        if (accessed && l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] < L1I_CONFIDENCE_COUNTER_MAX_VALUE) {
          l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k]++;
        }
        if (!accessed && l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k] > 0) {
          l1i_entangled_table[l1i_cpu_id][set][way].entangled_conf[k]--;
        }
      }
    }
  }
}

// EXTRA PREFETCH QUEUE

#define L1I_XPQ_ENTRIES 32
#define L1I_XPQ_MASK (L1I_XPQ_ENTRIES - 1)

typedef struct __l1i_xpq_entry {
  uint64_t line_addr;      // 58 bits
  uint64_t entangled_addr; // 58 bits
  uint32_t bb_size;        // L1I_MERGE_BBSIZE_BITS bits
} l1i_xpq_entry;

l1i_xpq_entry l1i_xpq[NUM_CPUS][L1I_XPQ_ENTRIES];
uint64_t l1i_xpq_head[NUM_CPUS]; // log_2 (L1I_XPQ_ENTRIES)

void l1i_init_xpq()
{
  l1i_xpq_head[l1i_cpu_id] = 0;
  for (uint32_t i = 0; i < L1I_XPQ_ENTRIES; i++) {
    l1i_xpq[l1i_cpu_id][i].line_addr = 0;
    l1i_xpq[l1i_cpu_id][i].entangled_addr = 0;
    l1i_xpq[l1i_cpu_id][i].bb_size = 0;
  }
}

void l1i_add_xpq(uint64_t line_addr, uint64_t entangled_addr, uint32_t bb_size)
{
  assert(bb_size > 0);

  // Merge if possible
  // 这是一个循环队列, 这是在看是否存在相同的请求
  uint32_t first = (l1i_xpq_head[l1i_cpu_id] + L1I_XPQ_MASK) % L1I_XPQ_ENTRIES;
  for (uint32_t count = 0, i = first; count < L1I_XPQ_ENTRIES; count++, i = (i + L1I_XPQ_MASK) % L1I_XPQ_ENTRIES) {
    if (l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].bb_size && line_addr == l1i_xpq[l1i_cpu_id][i].line_addr) {
      if (l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].bb_size < bb_size) {
        l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].bb_size = bb_size;
        return;
      }
    }
  }

  // 将新的请求插到队列头
  l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].line_addr = line_addr;
  l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].entangled_addr = entangled_addr;
  l1i_xpq[l1i_cpu_id][l1i_xpq_head[l1i_cpu_id]].bb_size = bb_size;
  l1i_xpq_head[l1i_cpu_id] = (l1i_xpq_head[l1i_cpu_id] + 1) % L1I_XPQ_ENTRIES;
}

bool l1i_empty_xpq() { return l1i_xpq[l1i_cpu_id][(l1i_xpq_head[l1i_cpu_id] + L1I_XPQ_MASK) % L1I_XPQ_ENTRIES].bb_size == 0; }

// Returns next line to prefetch
uint64_t l1i_get_xpq(uint64_t& entangled_addr)
{
  assert(!l1i_empty_xpq());

  // find tail
  uint32_t tail;
  for (tail = l1i_xpq_head[l1i_cpu_id]; tail != (l1i_xpq_head[l1i_cpu_id] + L1I_XPQ_MASK) % L1I_XPQ_ENTRIES; tail = (tail + 1) % L1I_XPQ_ENTRIES) {
    if (l1i_xpq[l1i_cpu_id][tail].bb_size) {
      break;
    }
  }

  // get address to prefetch
  uint64_t pf_addr = l1i_xpq[l1i_cpu_id][tail].line_addr;
  entangled_addr = l1i_xpq[l1i_cpu_id][tail].entangled_addr;

  // update queue
  l1i_xpq[l1i_cpu_id][tail].bb_size--;
  if (l1i_xpq[l1i_cpu_id][tail].bb_size == 0) {
    return pf_addr;
  }
  l1i_xpq[l1i_cpu_id][tail].line_addr++;
  l1i_xpq[l1i_cpu_id][tail].entangled_addr = 0;
  return pf_addr;
}

// INTERFACE

void O3_CPU::prefetcher_initialize()
{
  cout << "CPU " << cpu << " EPI prefetcher" << endl;

  l1i_cpu_id = cpu;
  l1i_last_basic_block = 0;
  l1i_consecutive_count = 0;
  l1i_basic_block_merge_diff = 0;

  l1i_init_hist_table();
  l1i_init_timing_tables();
  l1i_init_entangled_table();
  l1i_init_xpq();
}

void O3_CPU::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {}

uint32_t O3_CPU::prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit, uint32_t metadata_in)
{
  l1i_cpu_id = cpu;
  uint64_t line_addr = v_addr >> LOG2_BLOCK_SIZE;

  // 处理连续访问的部分
  bool consecutive = false;
  if (l1i_last_basic_block + l1i_consecutive_count == line_addr) { // Same
    return metadata_in;
  } else if (l1i_last_basic_block + l1i_consecutive_count + 1 == line_addr) { // Consecutive
    l1i_consecutive_count++;
    consecutive = true;
  }

  // Queue basic block prefetches
  uint32_t bb_size = l1i_get_bbsize_entangled_table(line_addr);
  if (bb_size > 0) {
    l1i_add_xpq(line_addr + 1, 0, bb_size);
  }

  // Queue entangled and basic block of entangled prefetches
  for (uint32_t k = 0; k < L1I_MAX_ENTANGLED_PER_LINE; k++) {
    uint64_t entangled_addr = l1i_get_entangled_addr_entangled_table(line_addr, k);
    if (entangled_addr && (entangled_addr != line_addr)) {
      uint32_t bb_size = l1i_get_bbsize_entangled_table(entangled_addr);
      l1i_add_xpq(entangled_addr, line_addr, bb_size + 1);
    }
  }

  if (!consecutive) { // New basic block found
    uint32_t max_bb_size = l1i_get_bbsize_entangled_table(l1i_last_basic_block);

    // Check for merging bb opportunities
    if (l1i_consecutive_count) { // single blocks no need to merge
      // 这里用的是上次计算的结果
      if (l1i_basic_block_merge_diff > 0) {
        // 那这里计算的其实还是basic addr
        l1i_add_bbsize_table(l1i_last_basic_block - l1i_basic_block_merge_diff, l1i_consecutive_count + l1i_basic_block_merge_diff);
        l1i_add_bb_size_hist_table(l1i_last_basic_block - l1i_basic_block_merge_diff, l1i_consecutive_count + l1i_basic_block_merge_diff);
      } else {
        l1i_add_bbsize_table(l1i_last_basic_block, max(max_bb_size, l1i_consecutive_count));
        l1i_add_bb_size_hist_table(l1i_last_basic_block, max(max_bb_size, l1i_consecutive_count));
      }
    }
  }

  if (!consecutive) { // New basic block found
    l1i_consecutive_count = 0;
    l1i_last_basic_block = line_addr;
  }

  if (!consecutive) {
    l1i_basic_block_merge_diff = l1i_find_bb_merge_hist_table(l1i_last_basic_block);
  }

  // Add the request in the history buffer
  if (!consecutive && l1i_basic_block_merge_diff == 0) {
    if ((l1i_find_hist_entry(line_addr) == L1I_HIST_TABLE_ENTRIES)) {
      l1i_add_hist_table(line_addr);
    } else {
      if (!cache_hit && !l1i_ongoing_accessed_request(line_addr)) {
        l1i_add_hist_table(line_addr);
      }
    }
  }

  // Add miss in the latency table
  // 这里是反馈机制的一部分，记录发射的pf_addr或当期line_addr是否hit
  if (!cache_hit && !l1i_ongoing_request(line_addr)) {
    DEBUG(cout << "Record Miss, PQ occupy: " << L1I_bus.lower_level->get_occupancy(4, 0) << ", MSHR occupy: " << L1I_bus.lower_level->get_occupancy(0, 0) << endl;)
    l1i_add_timing_entry(line_addr, 0);
    l1i_access_timing_entry(line_addr);
  } else {
    l1i_access_timing_entry(line_addr);
  }

  // Do prefetches
  while (L1I_bus.lower_level->get_occupancy(4, 0) + L1I_bus.lower_level->get_occupancy(3, 0) < L1I_PQ_SIZE && !l1i_empty_xpq()) {
    uint64_t entangled_addr = 0;
    uint64_t pf_line_addr = l1i_get_xpq(entangled_addr);
    uint64_t pf_addr = (pf_line_addr << LOG2_BLOCK_SIZE);
    if (!l1i_ongoing_request(pf_line_addr)) {
      // FIXME zeal4u: pf can fail to issue
      prefetch_code_line(pf_addr);
      DEBUG(cout << "Record Prefetch: " << pf_addr << ", VAPQ occupy: " << L1I_bus.lower_level->get_occupancy(4, 0) << ", PQ occupy: " << L1I_bus.lower_level->get_occupancy(3, 0) <<", MSHR occupy: " << L1I_bus.lower_level->get_occupancy(0, 0) << endl;)
      l1i_add_timing_entry(pf_line_addr, entangled_addr);
    }
  }

  return metadata_in;
}

void O3_CPU::prefetcher_cycle_operate()
{
  // Do prefetches
  // 这一个cycle发射这么多请求？
  while (L1I_bus.lower_level->get_occupancy(4, 0) + L1I_bus.lower_level->get_occupancy(3, 0) < L1I_PQ_SIZE && !l1i_empty_xpq()) {
    uint64_t entangled_addr = 0;
    uint64_t pf_line_addr = l1i_get_xpq(entangled_addr);
    uint64_t pf_addr = (pf_line_addr << LOG2_BLOCK_SIZE);
    if (!l1i_ongoing_request(pf_line_addr)) {
      prefetch_code_line(pf_addr);
      DEBUG(cout << "Record Prefetch: " << pf_addr << ", VAPQ occupy: " << L1I_bus.lower_level->get_occupancy(4, 0) << ", PQ occupy: " << L1I_bus.lower_level->get_occupancy(3, 0) <<", MSHR occupy: " << L1I_bus.lower_level->get_occupancy(0, 0) << endl;)
      l1i_add_timing_entry(pf_line_addr, entangled_addr);
    }
  }
}

uint32_t O3_CPU::prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr, uint32_t metadata_in)
{
  l1i_cpu_id = cpu;
  uint64_t line_addr = (v_addr >> LOG2_BLOCK_SIZE);
  uint64_t evicted_line_addr = (evicted_v_addr >> LOG2_BLOCK_SIZE);

  // Line is in cache
  if (evicted_v_addr) {
    uint64_t bere_line_addr = 0;
    bool accessed = l1i_invalid_timing_cache_entry(evicted_line_addr, bere_line_addr);
    if (bere_line_addr != 0) { // it is not original miss
      // If accessed hit, but if not wrong
      l1i_update_confidence_entangled_table(bere_line_addr, evicted_line_addr, accessed);
    }
  }

  uint64_t latency = l1i_get_latency_timing_mshr(line_addr);

  l1i_move_timing_entry(line_addr);

  // Get and update entangled
  if (latency) {
    bool inserted = false;
    for (uint32_t i = 0; i < L1I_TRIES_AVAIL_ENTANGLED; i++) {
      uint64_t bere = l1i_get_bere_hist_table(line_addr, latency, i);
      if (bere && line_addr != bere) {
        if (l1i_avail_entangled_table(bere, line_addr, false)) {
          l1i_add_entangled_table(bere, line_addr);
          inserted = true;
          break;
        }
      }
    }
    if (!inserted) {
      for (uint32_t i = 0; i < L1I_TRIES_AVAIL_ENTANGLED_NOT_PRESENT; i++) {
        uint64_t bere = l1i_get_bere_hist_table(line_addr, latency, i);
        if (bere && line_addr != bere) {
          if (l1i_avail_entangled_table(bere, line_addr, true)) {
            l1i_add_entangled_table(bere, line_addr);
            inserted = true;
            break;
          }
        }
      }
    }
    if (!inserted) {
      uint64_t bere = l1i_get_bere_hist_table(line_addr, latency);
      if (bere && line_addr != bere) {
        l1i_add_entangled_table(bere, line_addr);
      }
    }
  }
  return metadata_in;
}

void O3_CPU::prefetcher_final_stats() { cout << "CPU " << cpu << " L1I EPI prefetcher final stats" << endl; }