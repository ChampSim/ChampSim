// Code analyzed for the Third Data Prefetching Championship
//
// Author: Alberto Ros, University of Murcia
//
// Paper #13: Berti: A Per-Page Best-Request-Time Delta Prefetcher

#include "cache.h"
#include <cassert>
#include <iostream>
#include <ostream>

using namespace std;

#define L1D_PAGE_BLOCKS_BITS (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)
#define L1D_PAGE_BLOCKS (1 << L1D_PAGE_BLOCKS_BITS)
#define L1D_PAGE_OFFSET_MASK (L1D_PAGE_BLOCKS - 1)

#define L1D_MAX_NUM_BURST_PREFETCHES 3

#define L1D_BERTI_CTR_MED_HIGH_CONFIDENCE 2

// To access cpu in my functions
uint32_t l1d_cpu_id;

// TIME AND OVERFLOWS

#define L1D_TIME_BITS 16
#define L1D_TIME_OVERFLOW ((uint64_t)1 << L1D_TIME_BITS)
#define L1D_TIME_MASK (L1D_TIME_OVERFLOW - 1)

uint64_t l1d_get_latency(uint64_t cycle, uint64_t cycle_prev)
{
  return cycle - cycle_prev;
  uint64_t cycle_masked = cycle & L1D_TIME_MASK;
  uint64_t cycle_prev_masked = cycle_prev & L1D_TIME_MASK;
  if (cycle_prev_masked > cycle_masked) {
    return (cycle_masked + L1D_TIME_OVERFLOW) - cycle_prev_masked;
  }
  return cycle_masked - cycle_prev_masked;
}

// STRIDE

int l1d_calculate_stride(uint64_t prev_offset, uint64_t current_offset)
{
  int stride;
  if (current_offset > prev_offset) {
    stride = current_offset - prev_offset;
  } else {
    stride = prev_offset - current_offset;
    stride *= -1;
  }
  return stride;
}

// CURRENT PAGES TABLE

#define L1D_CURRENT_PAGES_TABLE_INDEX_BITS 6
#define L1D_CURRENT_PAGES_TABLE_ENTRIES ((1 << L1D_CURRENT_PAGES_TABLE_INDEX_BITS) - 1) // Null pointer for prev_request
#define L1D_CURRENT_PAGES_TABLE_NUM_BERTI 10
#define L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS 7

typedef struct __l1d_current_page_entry {
  uint64_t page_addr;                                    // 52 bits
  uint64_t ip;                                           // 10 bits
  uint64_t u_vector;                                     // 64 bits
  uint64_t first_offset;                                 // 6 bits
  int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI];          // 70 bits
  unsigned berti_ctr[L1D_CURRENT_PAGES_TABLE_NUM_BERTI]; // 60 bits
  uint64_t last_burst;                                   // 6 bits
  uint64_t lru;                                          // 6 bits
} l1d_current_page_entry;

l1d_current_page_entry l1d_current_pages_table[NUM_CPUS][L1D_CURRENT_PAGES_TABLE_ENTRIES];

void l1d_init_current_pages_table()
{
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    l1d_current_pages_table[l1d_cpu_id][i].page_addr = 0;
    l1d_current_pages_table[l1d_cpu_id][i].ip = 0;
    l1d_current_pages_table[l1d_cpu_id][i].u_vector = 0; // not valid
    l1d_current_pages_table[l1d_cpu_id][i].last_burst = 0;
    l1d_current_pages_table[l1d_cpu_id][i].lru = i;
  }
}

uint64_t l1d_get_current_pages_entry(uint64_t page_addr)
{
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][i].page_addr == page_addr)
      return i;
  }
  return L1D_CURRENT_PAGES_TABLE_ENTRIES;
}

void l1d_update_lru_current_pages_table(uint64_t index)
{
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][i].lru < l1d_current_pages_table[l1d_cpu_id][index].lru) { // Found
      l1d_current_pages_table[l1d_cpu_id][i].lru++;
    }
  }
  l1d_current_pages_table[l1d_cpu_id][index].lru = 0;
}

uint64_t l1d_get_lru_current_pages_entry()
{
  uint64_t lru = L1D_CURRENT_PAGES_TABLE_ENTRIES;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    l1d_current_pages_table[l1d_cpu_id][i].lru++;
    if (l1d_current_pages_table[l1d_cpu_id][i].lru == L1D_CURRENT_PAGES_TABLE_ENTRIES) {
      l1d_current_pages_table[l1d_cpu_id][i].lru = 0;
      lru = i;
    }
  }
  assert(lru != L1D_CURRENT_PAGES_TABLE_ENTRIES);
  return lru;
}

int l1d_get_berti_current_pages_table(uint64_t index, uint64_t& ctr)
{
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  uint64_t max_score = 0;
  uint64_t berti = 0;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    uint64_t score;
    score = l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i];
    if (score > max_score) {
      berti = l1d_current_pages_table[l1d_cpu_id][index].berti[i];
      max_score = score;
      ctr = l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i];
    }
  }
  return berti;
}

void l1d_add_current_pages_table(uint64_t index, uint64_t page_addr, uint64_t ip, uint64_t offset)
{
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  l1d_current_pages_table[l1d_cpu_id][index].page_addr = page_addr;
  l1d_current_pages_table[l1d_cpu_id][index].ip = ip;
  l1d_current_pages_table[l1d_cpu_id][index].u_vector = (uint64_t)1 << offset;
  l1d_current_pages_table[l1d_cpu_id][index].first_offset = offset;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i] = 0;
  }
  l1d_current_pages_table[l1d_cpu_id][index].last_burst = 0;
}

uint64_t l1d_update_demand_current_pages_table(uint64_t index, uint64_t offset)
{
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  l1d_current_pages_table[l1d_cpu_id][index].u_vector |= (uint64_t)1 << offset;
  l1d_update_lru_current_pages_table(index);
  return l1d_current_pages_table[l1d_cpu_id][index].ip;
}

void l1d_add_berti_current_pages_table(uint64_t index, int berti)
{
  assert(berti != 0);
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i] == 0) {
      l1d_current_pages_table[l1d_cpu_id][index].berti[i] = berti;
      l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i] = 1;
      break;
    } else if (l1d_current_pages_table[l1d_cpu_id][index].berti[i] == berti) {
      l1d_current_pages_table[l1d_cpu_id][index].berti_ctr[i]++;
      break;
    }
  }
  l1d_update_lru_current_pages_table(index);
}

bool l1d_requested_offset_current_pages_table(uint64_t index, uint64_t offset)
{
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  return l1d_current_pages_table[l1d_cpu_id][index].u_vector & ((uint64_t)1 << offset);
}

void l1d_remove_current_table_entry(uint64_t index)
{
  l1d_current_pages_table[l1d_cpu_id][index].page_addr = 0;
  l1d_current_pages_table[l1d_cpu_id][index].u_vector = 0;
  l1d_current_pages_table[l1d_cpu_id][index].berti[0] = 0;
}

// PREVIOUS REQUESTS TABLE

#define L1D_PREV_REQUESTS_TABLE_INDEX_BITS 10
#define L1D_PREV_REQUESTS_TABLE_ENTRIES (1 << L1D_PREV_REQUESTS_TABLE_INDEX_BITS)
#define L1D_PREV_REQUESTS_TABLE_MASK (L1D_PREV_REQUESTS_TABLE_ENTRIES - 1)
#define L1D_PREV_REQUESTS_TABLE_NULL_POINTER L1D_CURRENT_PAGES_TABLE_ENTRIES

typedef struct __l1d_prev_request_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset;            // 6 bits
  uint64_t time;              // 16 bits
} l1d_prev_request_entry;

l1d_prev_request_entry l1d_prev_requests_table[NUM_CPUS][L1D_PREV_REQUESTS_TABLE_ENTRIES];
uint64_t l1d_prev_requests_table_head[NUM_CPUS];

void l1d_init_prev_requests_table()
{
  l1d_prev_requests_table_head[l1d_cpu_id] = 0;
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_REQUESTS_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_find_prev_request_entry(uint64_t pointer, uint64_t offset)
{
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer && l1d_prev_requests_table[l1d_cpu_id][i].offset == offset)
      return i;
  }
  return L1D_PREV_REQUESTS_TABLE_ENTRIES;
}

void l1d_add_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle)
{
  // First find for coalescing
  if (l1d_find_prev_request_entry(pointer, offset) != L1D_PREV_REQUESTS_TABLE_ENTRIES)
    return;

  // Allocate a new entry (evict old one if necessary)
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].page_addr_pointer = pointer;
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].offset = offset;
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].time = cycle & L1D_TIME_MASK;
  l1d_prev_requests_table_head[l1d_cpu_id] = (l1d_prev_requests_table_head[l1d_cpu_id] + 1) & L1D_PREV_REQUESTS_TABLE_MASK;
}

void l1d_reset_pointer_prev_requests(uint64_t pointer)
{
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
      l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_REQUESTS_TABLE_NULL_POINTER;
    }
  }
}

uint64_t l1d_get_latency_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle)
{
  uint64_t index = l1d_find_prev_request_entry(pointer, offset);
  if (index == L1D_PREV_REQUESTS_TABLE_ENTRIES)
    return 0;
  return l1d_get_latency(cycle, l1d_prev_requests_table[l1d_cpu_id][index].time);
}

void l1d_get_berti_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle, int* berti)
{
  int my_pos = 0;
  uint64_t extra_time = 0;
  uint64_t last_time =
      l1d_prev_requests_table[l1d_cpu_id][(l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK].time;
  for (uint64_t i = (l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK;
       i != l1d_prev_requests_table_head[l1d_cpu_id]; i = (i + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK) {
    // Against the time overflow
    if (last_time < l1d_prev_requests_table[l1d_cpu_id][i].time) {
      extra_time = L1D_TIME_OVERFLOW;
    }
    last_time = l1d_prev_requests_table[l1d_cpu_id][i].time;
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
      if (l1d_prev_requests_table[l1d_cpu_id][i].time <= (cycle & L1D_TIME_MASK) + extra_time) {
        berti[my_pos] = l1d_calculate_stride(l1d_prev_requests_table[l1d_cpu_id][i].offset, offset);
        my_pos++;
        if (my_pos == L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS)
          return;
      }
    }
  }
  berti[my_pos] = 0;
}

// PREVIOUS PREFETCHES TABLE

#define L1D_PREV_PREFETCHES_TABLE_INDEX_BITS 9
#define L1D_PREV_PREFETCHES_TABLE_ENTRIES (1 << L1D_PREV_PREFETCHES_TABLE_INDEX_BITS)
#define L1D_PREV_PREFETCHES_TABLE_MASK (L1D_PREV_PREFETCHES_TABLE_ENTRIES - 1)
#define L1D_PREV_PREFETCHES_TABLE_NULL_POINTER L1D_CURRENT_PAGES_TABLE_ENTRIES

// We do not have access to the MSHR, so we aproximate it using this structure.
typedef struct __l1d_prev_prefetch_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset;            // 6 bits
  uint64_t time_lat;          // 16 bits // time if not completed, latency if completed
  bool completed;             // 1 bit
} l1d_prev_prefetch_entry;

l1d_prev_prefetch_entry l1d_prev_prefetches_table[NUM_CPUS][L1D_PREV_PREFETCHES_TABLE_ENTRIES];
uint64_t l1d_prev_prefetches_table_head[NUM_CPUS];

void l1d_init_prev_prefetches_table()
{
  l1d_prev_prefetches_table_head[l1d_cpu_id] = 0;
  for (int i = 0; i < L1D_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    l1d_prev_prefetches_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_PREFETCHES_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_find_prev_prefetch_entry(uint64_t pointer, uint64_t offset)
{
  for (int i = 0; i < L1D_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    if (l1d_prev_prefetches_table[l1d_cpu_id][i].page_addr_pointer == pointer && l1d_prev_prefetches_table[l1d_cpu_id][i].offset == offset)
      return i;
  }
  return L1D_PREV_PREFETCHES_TABLE_ENTRIES;
}

void l1d_add_prev_prefetches_table(uint64_t pointer, uint64_t offset, uint64_t cycle)
{
  // First find for coalescing
  if (l1d_find_prev_prefetch_entry(pointer, offset) != L1D_PREV_PREFETCHES_TABLE_ENTRIES)
    return;

  // Allocate a new entry (evict old one if necessary)
  l1d_prev_prefetches_table[l1d_cpu_id][l1d_prev_prefetches_table_head[l1d_cpu_id]].page_addr_pointer = pointer;
  l1d_prev_prefetches_table[l1d_cpu_id][l1d_prev_prefetches_table_head[l1d_cpu_id]].offset = offset;
  l1d_prev_prefetches_table[l1d_cpu_id][l1d_prev_prefetches_table_head[l1d_cpu_id]].time_lat = cycle & L1D_TIME_MASK;
  l1d_prev_prefetches_table[l1d_cpu_id][l1d_prev_prefetches_table_head[l1d_cpu_id]].completed = false;
  l1d_prev_prefetches_table_head[l1d_cpu_id] = (l1d_prev_prefetches_table_head[l1d_cpu_id] + 1) & L1D_PREV_PREFETCHES_TABLE_MASK;
}

void l1d_reset_pointer_prev_prefetches(uint64_t pointer)
{
  for (int i = 0; i < L1D_PREV_PREFETCHES_TABLE_ENTRIES; i++) {
    if (l1d_prev_prefetches_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
      l1d_prev_prefetches_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_PREFETCHES_TABLE_NULL_POINTER;
    }
  }
}

void l1d_reset_entry_prev_prefetches_table(uint64_t pointer, uint64_t offset)
{
  uint64_t index = l1d_find_prev_prefetch_entry(pointer, offset);
  if (index != L1D_PREV_PREFETCHES_TABLE_ENTRIES) {
    l1d_prev_prefetches_table[l1d_cpu_id][index].page_addr_pointer = L1D_PREV_PREFETCHES_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_get_and_set_latency_prev_prefetches_table(uint64_t pointer, uint64_t offset, uint64_t cycle)
{
  uint64_t index = l1d_find_prev_prefetch_entry(pointer, offset);
  if (index == L1D_PREV_PREFETCHES_TABLE_ENTRIES)
    return 0;
  if (!l1d_prev_prefetches_table[l1d_cpu_id][index].completed) {
    l1d_prev_prefetches_table[l1d_cpu_id][index].time_lat = l1d_get_latency(cycle, l1d_prev_prefetches_table[l1d_cpu_id][index].time_lat);
    l1d_prev_prefetches_table[l1d_cpu_id][index].completed = true;
  }
  return l1d_prev_prefetches_table[l1d_cpu_id][index].time_lat;
}

uint64_t l1d_get_latency_prev_prefetches_table(uint64_t pointer, uint64_t offset)
{
  uint64_t index = l1d_find_prev_prefetch_entry(pointer, offset);
  if (index == L1D_PREV_PREFETCHES_TABLE_ENTRIES)
    return 0;
  if (!l1d_prev_prefetches_table[l1d_cpu_id][index].completed)
    return 0;
  return l1d_prev_prefetches_table[l1d_cpu_id][index].time_lat;
}

// RECORD PAGES TABLE

// #define L1D_RECORD_PAGES_TABLE_INDEX_BITS 10
#define L1D_RECORD_PAGES_TABLE_ENTRIES (((1 << 11) + (1 << 8)) - 1) // ((1 << L1D_RECORD_PAGES_TABLE_INDEX_BITS) - 1) // Null pointer for ip table
#define L1D_TRUNCATED_PAGE_ADDR_BITS 32                             // 4 bytes
#define L1D_TRUNCATED_PAGE_ADDR_MASK (((uint64_t)1 << L1D_TRUNCATED_PAGE_ADDR_BITS) - 1)

typedef struct __l1d_record_page_entry {
  uint64_t page_addr;    // 4 bytes
  uint64_t u_vector;     // 8 bytes
  uint64_t first_offset; // 6 bits
  int berti;             // 7 bits
  uint64_t lru;          // 10 bits
} l1d_record_page_entry;

l1d_record_page_entry l1d_record_pages_table[NUM_CPUS][L1D_RECORD_PAGES_TABLE_ENTRIES];

void l1d_init_record_pages_table()
{
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    l1d_record_pages_table[l1d_cpu_id][i].page_addr = 0;
    l1d_record_pages_table[l1d_cpu_id][i].u_vector = 0;
    l1d_record_pages_table[l1d_cpu_id][i].lru = i;
  }
}

uint64_t l1d_get_lru_record_pages_entry()
{
  uint64_t lru = L1D_RECORD_PAGES_TABLE_ENTRIES;
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    l1d_record_pages_table[l1d_cpu_id][i].lru++;
    if (l1d_record_pages_table[l1d_cpu_id][i].lru == L1D_RECORD_PAGES_TABLE_ENTRIES) {
      l1d_record_pages_table[l1d_cpu_id][i].lru = 0;
      lru = i;
    }
  }
  assert(lru != L1D_RECORD_PAGES_TABLE_ENTRIES);
  return lru;
}

void l1d_update_lru_record_pages_table(uint64_t index)
{
  assert(index < L1D_RECORD_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].lru < l1d_record_pages_table[l1d_cpu_id][index].lru) { // Found
      l1d_record_pages_table[l1d_cpu_id][i].lru++;
    }
  }
  l1d_record_pages_table[l1d_cpu_id][index].lru = 0;
}

void l1d_add_record_pages_table(uint64_t index, uint64_t page_addr, uint64_t vector, uint64_t first_offset, int berti)
{
  assert(index < L1D_RECORD_PAGES_TABLE_ENTRIES);
  l1d_record_pages_table[l1d_cpu_id][index].page_addr = page_addr & L1D_TRUNCATED_PAGE_ADDR_MASK;
  l1d_record_pages_table[l1d_cpu_id][index].u_vector = vector;
  l1d_record_pages_table[l1d_cpu_id][index].first_offset = first_offset;
  l1d_record_pages_table[l1d_cpu_id][index].berti = berti;
  l1d_update_lru_record_pages_table(index);
}

uint64_t l1d_get_entry_record_pages_table(uint64_t page_addr, uint64_t first_offset)
{
  uint64_t trunc_page_addr = page_addr & L1D_TRUNCATED_PAGE_ADDR_MASK;
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].page_addr == trunc_page_addr && l1d_record_pages_table[l1d_cpu_id][i].first_offset == first_offset) { // Found
      return i;
    }
  }
  return L1D_RECORD_PAGES_TABLE_ENTRIES;
}

uint64_t l1d_get_entry_record_pages_table(uint64_t page_addr)
{
  uint64_t trunc_page_addr = page_addr & L1D_TRUNCATED_PAGE_ADDR_MASK;
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].page_addr == trunc_page_addr) { // Found
      return i;
    }
  }
  return L1D_RECORD_PAGES_TABLE_ENTRIES;
}

void l1d_copy_entries_record_pages_table(uint64_t index_from, uint64_t index_to)
{
  assert(index_from < L1D_RECORD_PAGES_TABLE_ENTRIES);
  assert(index_to < L1D_RECORD_PAGES_TABLE_ENTRIES);
  l1d_record_pages_table[l1d_cpu_id][index_to].page_addr = l1d_record_pages_table[l1d_cpu_id][index_from].page_addr;
  l1d_record_pages_table[l1d_cpu_id][index_to].u_vector = l1d_record_pages_table[l1d_cpu_id][index_from].u_vector;
  l1d_record_pages_table[l1d_cpu_id][index_to].first_offset = l1d_record_pages_table[l1d_cpu_id][index_from].first_offset;
  l1d_record_pages_table[l1d_cpu_id][index_to].berti = l1d_record_pages_table[l1d_cpu_id][index_from].berti;
  l1d_update_lru_record_pages_table(index_to);
}

// IP TABLE

#define L1D_IP_TABLE_INDEX_BITS 10
#define L1D_IP_TABLE_ENTRIES (1 << L1D_IP_TABLE_INDEX_BITS)
#define L1D_IP_TABLE_INDEX_MASK (L1D_IP_TABLE_ENTRIES - 1)
#define L1D_IP_TABLE_NULL_POINTER L1D_RECORD_PAGES_TABLE_ENTRIES

uint64_t l1d_ip_table[NUM_CPUS][L1D_IP_TABLE_ENTRIES]; // 11 bits

void l1d_init_ip_table()
{
  for (int i = 0; i < L1D_IP_TABLE_ENTRIES; i++) {
    l1d_ip_table[l1d_cpu_id][i] = L1D_IP_TABLE_NULL_POINTER;
  }
}

// TABLE MOVEMENTS

// Sumarizes the content to the current page to be evicted
// From all timely requests found, we record the best
void l1d_record_current_page(uint64_t index_current)
{
  if (l1d_current_pages_table[l1d_cpu_id][index_current].u_vector) { // Valid entry
    uint64_t record_index = l1d_ip_table[l1d_cpu_id][l1d_current_pages_table[l1d_cpu_id][index_current].ip & L1D_IP_TABLE_INDEX_MASK];
    assert(record_index < L1D_RECORD_PAGES_TABLE_ENTRIES);
    uint64_t confidence;
    l1d_add_record_pages_table(record_index, l1d_current_pages_table[l1d_cpu_id][index_current].page_addr,
                               l1d_current_pages_table[l1d_cpu_id][index_current].u_vector, l1d_current_pages_table[l1d_cpu_id][index_current].first_offset,
                               l1d_get_berti_current_pages_table(index_current, confidence));
  }
}

// INTERFACE

void CACHE::prefetcher_initialize()
{
  l1d_cpu_id = cpu;
  cout << "CPU " << cpu << " L1D Berti prefetcher" << endl;

  l1d_init_current_pages_table();
  l1d_init_prev_requests_table();
  l1d_init_prev_prefetches_table();
  l1d_init_record_pages_table();
  l1d_init_ip_table();
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint64_t instr_id, uint8_t cache_hit, bool prefetch_hit, uint8_t type, uint32_t metadata_in)
{
  l1d_cpu_id = cpu;
  uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;
  uint64_t page_addr = line_addr >> L1D_PAGE_BLOCKS_BITS;
  uint64_t offset = line_addr & L1D_PAGE_OFFSET_MASK;

  // Update current pages table
  // Find the entry
  uint64_t index = l1d_get_current_pages_entry(page_addr);

  // If not accessed recently
  if (index == L1D_CURRENT_PAGES_TABLE_ENTRIES || !l1d_requested_offset_current_pages_table(index, offset)) {
    // cout << "OFFSETS: " << hex << ip << " " << page_addr << " " << dec << offset << endl;

    if (index < L1D_CURRENT_PAGES_TABLE_ENTRIES) { // Found
      // cout << " FOUND" << endl;

      // If offset found, already requested, so return;
      if (l1d_requested_offset_current_pages_table(index, offset))
        return metadata_in;

      uint64_t first_ip = l1d_update_demand_current_pages_table(index, offset);
      assert(l1d_ip_table[l1d_cpu_id][first_ip & L1D_IP_TABLE_INDEX_MASK] != L1D_IP_TABLE_NULL_POINTER);

      // Update berti
      if (cache_hit) {
        uint64_t pref_latency = l1d_get_latency_prev_prefetches_table(index, offset);
        if (pref_latency != 0) {
          // Find berti distance from pref_latency cycles before
          int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
          l1d_get_berti_prev_requests_table(index, offset, current_cycle - pref_latency, berti);
          for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS; i++) {
            if (berti[i] == 0)
              break;
            assert(abs(berti[i]) < L1D_PAGE_BLOCKS);
            l1d_add_berti_current_pages_table(index, berti[i]);
          }

          // Eliminate a prev prefetch since it has been used
          l1d_reset_entry_prev_prefetches_table(index, offset);
        }
      }

      if (first_ip != ip) {
        // Assign same pointer to group IPs
        l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] = l1d_ip_table[l1d_cpu_id][first_ip & L1D_IP_TABLE_INDEX_MASK];
      }
    } else { // Not found: Add entry

      // Find victim and clear pointers to it
      uint64_t victim_index = l1d_get_lru_current_pages_entry(); // already updates lru
      assert(victim_index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
      l1d_reset_pointer_prev_requests(victim_index);   // Not valid anymore
      l1d_reset_pointer_prev_prefetches(victim_index); // Not valid anymore

      // Copy victim to record table
      l1d_record_current_page(victim_index);

      // Add new current page
      index = victim_index;
      l1d_add_current_pages_table(index, page_addr, ip & L1D_IP_TABLE_INDEX_MASK, offset);

      // Set pointer in IP table
      uint64_t index_record = l1d_get_entry_record_pages_table(page_addr, offset);
      // The ip pointer is null
      if (l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] == L1D_IP_TABLE_NULL_POINTER) {
        if (index_record == L1D_RECORD_PAGES_TABLE_ENTRIES) { // Page not recorded
          // Get free record page pointer.
          uint64_t new_pointer = l1d_get_lru_record_pages_entry();
          l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] = new_pointer;
        } else {
          l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] = index_record;
        }
      } else if (l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] != index_record) {
        // If the current IP is valid, but points to another address
        // we replicate it in another record entry (lru)
        // such that the recorded page is not deleted when the current entry summarizes
        uint64_t new_pointer = l1d_get_lru_record_pages_entry();
        l1d_copy_entries_record_pages_table(l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK], new_pointer);
        l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK] = new_pointer;
      }
    }

    l1d_add_prev_requests_table(index, offset, current_cycle);

    // PREDICT
    uint64_t u_vector = 0;
    uint64_t first_offset = l1d_current_pages_table[l1d_cpu_id][index].first_offset;
    int berti = 0;
    bool recorded = false;

    uint64_t ip_pointer = l1d_ip_table[l1d_cpu_id][ip & L1D_IP_TABLE_INDEX_MASK];
    uint64_t pgo_pointer = l1d_get_entry_record_pages_table(page_addr, first_offset);
    uint64_t pg_pointer = l1d_get_entry_record_pages_table(page_addr);
    uint64_t berti_confidence = 0;
    int current_berti = l1d_get_berti_current_pages_table(index, berti_confidence);
    uint64_t match_confidence = 0;

    // If match with current page+first_offset, use record
    if (pgo_pointer != L1D_RECORD_PAGES_TABLE_ENTRIES
        && (l1d_record_pages_table[l1d_cpu_id][pgo_pointer].u_vector | l1d_current_pages_table[l1d_cpu_id][index].u_vector)
               == l1d_record_pages_table[l1d_cpu_id][pgo_pointer].u_vector) {
      u_vector = l1d_record_pages_table[l1d_cpu_id][pgo_pointer].u_vector;
      berti = l1d_record_pages_table[l1d_cpu_id][pgo_pointer].berti;
      match_confidence = 1; // High confidence
      recorded = true;
    } else
      // If match with current ip+first_offset, use record
      if (l1d_record_pages_table[l1d_cpu_id][ip_pointer].first_offset == first_offset
          && (l1d_record_pages_table[l1d_cpu_id][ip_pointer].u_vector | l1d_current_pages_table[l1d_cpu_id][index].u_vector)
                 == l1d_record_pages_table[l1d_cpu_id][ip_pointer].u_vector) {
        u_vector = l1d_record_pages_table[l1d_cpu_id][ip_pointer].u_vector;
        berti = l1d_record_pages_table[l1d_cpu_id][ip_pointer].berti;
        match_confidence = 1; // High confidence
        recorded = true;
      } else
        // If no exact match, trust current if it has already a berti
        if (current_berti != 0 && berti_confidence >= L1D_BERTI_CTR_MED_HIGH_CONFIDENCE) { // Medium-High confidence
          u_vector = l1d_current_pages_table[l1d_cpu_id][index].u_vector;
          berti = current_berti;
        } else
          // If match with current page, use record
          if (pg_pointer != L1D_RECORD_PAGES_TABLE_ENTRIES) { // Medium confidence
            u_vector = l1d_record_pages_table[l1d_cpu_id][pg_pointer].u_vector;
            berti = l1d_record_pages_table[l1d_cpu_id][pg_pointer].berti;
            recorded = true;
          } else
            // If match with current ip, use record
            if (l1d_record_pages_table[l1d_cpu_id][ip_pointer].u_vector) { // Medium confidence
              u_vector = l1d_record_pages_table[l1d_cpu_id][ip_pointer].u_vector;
              berti = l1d_record_pages_table[l1d_cpu_id][ip_pointer].berti;
              recorded = true;
            }

    // Burst for the first access of a page or if pending bursts
    if (first_offset == offset || l1d_current_pages_table[l1d_cpu_id][index].last_burst != 0) {
      uint64_t first_burst;
      if (l1d_current_pages_table[l1d_cpu_id][index].last_burst != 0) {
        first_burst = l1d_current_pages_table[l1d_cpu_id][index].last_burst;
        l1d_current_pages_table[l1d_cpu_id][index].last_burst = 0;
      } else if (berti >= 0) {
        first_burst = offset + 1;
      } else {
        first_burst = offset - 1;
      }
      if (recorded && match_confidence) {
        int bursts = 0;
        if (berti > 0) {
          for (uint64_t i = first_burst; i < offset + berti; i++) {
            if (i >= L1D_PAGE_BLOCKS)
              break; // Stay in the page
            // Only if previously requested and not demanded
            uint64_t pf_line_addr = (page_addr << L1D_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !l1d_requested_offset_current_pages_table(index, pf_offset)) {
              if (get_pq_occupancy().back() < this->PQ_SIZE && bursts < L1D_MAX_NUM_BURST_PREFETCHES) {
                bool prefetched = prefetch_line(pf_addr, true, 0);
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  l1d_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                l1d_current_pages_table[l1d_cpu_id][index].last_burst = i;
                break;
              }
            }
          }
        } else if (berti < 0) {
          for (int i = first_burst; i > ((int)offset) + berti; i--) {
            if (i < 0)
              break; // Stay in the page
            // Only if previously requested and not demanded
            uint64_t pf_line_addr = (page_addr << L1D_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !l1d_requested_offset_current_pages_table(index, pf_offset)) {
              if (get_pq_occupancy().back() < get_pq_size().back() && bursts < L1D_MAX_NUM_BURST_PREFETCHES) {
                bool prefetched = prefetch_line(pf_addr, true, 0);
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  l1d_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                l1d_current_pages_table[l1d_cpu_id][index].last_burst = i;
                break;
              }
            }
          }
        } else { // berti == 0 (zig zag of all)
          for (int i = first_burst, j = (first_offset << 1) - i; i < L1D_PAGE_BLOCKS || j >= 0; i++, j = (first_offset << 1) - i) {
            // Only if previously requested and not demanded
            // Dir ++
            uint64_t pf_line_addr = (page_addr << L1D_PAGE_BLOCKS_BITS) | i;
            uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            uint64_t pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
            uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << i) & u_vector) && !l1d_requested_offset_current_pages_table(index, pf_offset)) {
              if (get_pq_occupancy().back() < get_pq_size().back() && bursts < L1D_MAX_NUM_BURST_PREFETCHES) {
                bool prefetched = prefetch_line(pf_addr, true, 0);
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  l1d_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else { // record last burst
                l1d_current_pages_table[l1d_cpu_id][index].last_burst = i;
                break;
              }
            }
            // Dir --
            pf_line_addr = (page_addr << L1D_PAGE_BLOCKS_BITS) | j;
            pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
            pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
            pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
            if ((((uint64_t)1 << j) & u_vector) && !l1d_requested_offset_current_pages_table(index, pf_offset)) {
              if (get_pq_occupancy().back() < get_pq_size().back() && bursts < L1D_MAX_NUM_BURST_PREFETCHES) {
                bool prefetched = prefetch_line(pf_addr, true, 0);
                if (prefetched) {
                  assert(pf_page_addr == page_addr);
                  l1d_add_prev_prefetches_table(index, pf_offset, current_cycle);
                  bursts++;
                }
              } else {
                // record only positive burst
              }
            }
          }
        }
      } else { // not recorded
      }
    }

    if (berti != 0) {
      uint64_t pf_line_addr = line_addr + berti;
      uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
      uint64_t pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
      uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
      if (!l1d_requested_offset_current_pages_table(index, pf_offset)          // Only prefetch if not demanded
          && (!match_confidence || (((uint64_t)1 << pf_offset) & u_vector))) { // And prev. accessed
        bool prefetched = prefetch_line(pf_addr, true, 0);
        if (prefetched) {
          cout << L1D_PAGE_BLOCKS_BITS << " " << L1D_PAGE_OFFSET_MASK << " " << hex << line_addr << " " << pf_line_addr << endl;
          assert(pf_page_addr == page_addr);
          l1d_add_prev_prefetches_table(index, pf_offset, current_cycle);
        }
      }
    }
  }
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  l1d_cpu_id = cpu;
  uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE);
  uint64_t page_addr = line_addr >> L1D_PAGE_BLOCKS_BITS;
  uint64_t offset = line_addr & L1D_PAGE_OFFSET_MASK;

  uint64_t pointer_prev = l1d_get_current_pages_entry(page_addr);

  if (pointer_prev < L1D_CURRENT_PAGES_TABLE_ENTRIES) { // Not found, not entry in prev requests
    uint64_t pref_latency = l1d_get_and_set_latency_prev_prefetches_table(pointer_prev, offset, current_cycle);
    uint64_t demand_latency = l1d_get_latency_prev_requests_table(pointer_prev, offset, current_cycle);

    // First look in prefetcher, since if there is a hit, it is the time the miss started
    // If no prefetch, then its latency is the demand one
    if (pref_latency == 0) {
      pref_latency = demand_latency;
    }

    if (demand_latency != 0) { // Not found, berti will not be found neither

      // Find berti (distance from pref_latency + demand_latency cycles before
      int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
      l1d_get_berti_prev_requests_table(pointer_prev, offset, current_cycle - (pref_latency + demand_latency), berti);
      for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS; i++) {
        if (berti[i] == 0)
          break;
        assert(abs(berti[i]) < L1D_PAGE_BLOCKS);
        l1d_add_berti_current_pages_table(pointer_prev, berti[i]);
      }
    }
  }

  uint64_t victim_index = l1d_get_current_pages_entry(evicted_addr >> LOG2_PAGE_SIZE);
  if (victim_index < L1D_CURRENT_PAGES_TABLE_ENTRIES) {
    // Copy victim to record table
    l1d_record_current_page(victim_index);
    l1d_remove_current_table_entry(victim_index);
  }
  return metadata_in;
}

void CACHE::prefetcher_final_stats() { cout << "CPU " << cpu << " L1D berti prefetcher final stats" << endl; }
void CACHE::prefetcher_squash(uint64_t ip, uint64_t instr_id) {}
void CACHE::prefetcher_cycle_operate() {}