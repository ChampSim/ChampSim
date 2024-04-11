#include "cache.h"
#include <iostream>
#include <ostream>
#include <cassert>

using namespace std;

#define L1D_PAGE_BLOCKS_BITS (LOG2_PAGE_SIZE - LOG2_BLOCK_SIZE)
#define L1D_PAGE_BLOCKS (1 << L1D_PAGE_BLOCKS_BITS)
#define L1D_PAGE_OFFSET_MASK (L1D_PAGE_BLOCKS - 1)

#define L1D_BERTI_THROTTLING 1
#define L1D_BURST_THROTTLING 7

#define L1D_BURST_THRESHOLD 0.99

#define CONTINUE_BURST
#define PREFETCH_FOR_LONG_REUSE
#define LONG_REUSE_LIMIT 16

//#define BERTI_LATENCIES
//#define JUST_BERTI // No compensation for holes
#define LINNEA
#define WARMUP_NEW_PAGES

// To access cpu in my functions
uint32_t l1d_cpu_id;

// TIME AND OVERFLOWS

#define L1D_TIME_BITS 16
#define L1D_TIME_OVERFLOW ((uint64_t)1 << L1D_TIME_BITS)
#define L1D_TIME_MASK (L1D_TIME_OVERFLOW - 1)

uint64_t l1d_get_latency(uint64_t cycle, uint64_t cycle_prev) {
  uint64_t cycle_masked = cycle & L1D_TIME_MASK;
  uint64_t cycle_prev_masked = cycle_prev & L1D_TIME_MASK;
  if (cycle_prev_masked > cycle_masked) {
    return (cycle_masked + L1D_TIME_OVERFLOW) - cycle_prev_masked;
  }
  return cycle_masked - cycle_prev_masked;
}

// STRIDE

int l1d_calculate_stride(uint64_t prev_offset, uint64_t current_offset) {
  assert(prev_offset < L1D_PAGE_BLOCKS);
  assert(current_offset < L1D_PAGE_BLOCKS);
  int stride;
  if (current_offset > prev_offset) {
    stride = current_offset - prev_offset;
  } else {
    stride = prev_offset - current_offset;
    stride *= -1;
  }
  assert(stride > (0 - L1D_PAGE_BLOCKS) && stride < L1D_PAGE_BLOCKS);
  return stride;
}

// BIT VECTOR

uint64_t l1d_count_bit_vector(uint64_t vector) {
  uint64_t count = 0;
  for (int i = 0; i < L1D_PAGE_BLOCKS; i++) {
    if (vector & ((uint64_t)1 << i)) {
      count++;
    }
  }
  return count;
}

uint64_t l1d_count_wrong_berti_bit_vector(uint64_t vector, int berti) {
  uint64_t wrong = 0;
  for (int i = 0; i < L1D_PAGE_BLOCKS; i++) {
    if (vector & ((uint64_t)1 << i)) {
      if (i + berti >= 0 && i + berti < L1D_PAGE_BLOCKS && !(vector & ((uint64_t)1 << (i + berti)))) { 
	wrong++;
      }
    }
  }
  return wrong;
}

uint64_t l1d_count_lost_berti_bit_vector(uint64_t vector, int berti) {
  uint64_t lost = 0;
  if (berti > 0) {
    for (int i = 0; i < berti; i++) {
      if (vector & ((uint64_t)1 << i)) {
        lost++;
      }
    }
  } else if (berti < 0) {
    for (int i = L1D_PAGE_OFFSET_MASK; i > L1D_PAGE_OFFSET_MASK + berti; i--) {
      if (vector & ((uint64_t)1 << i)) {
        lost++;
      }
    }
  }
  return lost;
}

// Check if all last blocks within berti where accessed
bool l1d_all_last_berti_accessed_bit_vector(uint64_t vector, int berti) {
  unsigned count_yes = 0;
  unsigned count_no = 0;
  if (berti < 0) {
    for (int i = 0; i < (0 - berti); i++) {
      (vector & ((uint64_t)1 << i)) ? count_yes++ : count_no++;
    }
  } else if (berti > 0) {
    for (int i = L1D_PAGE_OFFSET_MASK; i > L1D_PAGE_OFFSET_MASK - berti; i--) {
      (vector & ((uint64_t)1 << i)) ? count_yes++ : count_no++;
    }
  } else return true;
  //cout << "COUNT: " << count_yes << " " << count_no << " " << ((double)count_yes / (double)(count_yes + count_no)) << " " << (((double)count_yes / (double)(count_yes + count_no)) > L1D_BURST_THRESHOLD) << endl;
  if (count_yes == 0) return false;
  //return (count_no == 0);
  return ((double)count_yes / (double)(count_yes + count_no)) > L1D_BURST_THRESHOLD;
}

void l1d_print_bit_vector(uint64_t vector) {
  for (int i = 0; i < L1D_PAGE_BLOCKS; i++) {
    if (vector & ((uint64_t)1 << i)) cout << "1";
    else cout << "0";
  }
}

// CURRENT PAGES TABLE

#define L1D_CURRENT_PAGES_TABLE_INDEX_BITS 6
#define L1D_CURRENT_PAGES_TABLE_ENTRIES ((1 << L1D_CURRENT_PAGES_TABLE_INDEX_BITS) - 1) // Null pointer for prev_request
#define L1D_CURRENT_PAGES_TABLE_NUM_BERTI 8
#define L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS 8 // Better if not more than throttling

typedef struct __l1d_current_page_entry {
  uint64_t page_addr; // 52 bits
  uint64_t u_vector; // 64 bits
  int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI]; // 70 bits
  unsigned berti_score[L1D_CURRENT_PAGES_TABLE_NUM_BERTI]; // XXX bits
  int current_berti; // 7 bits
  int stride; // Divide tables. Long reuse do not need to calculate berties
  bool short_reuse; // 1 bit
  bool continue_burst; // 1 bit
  uint64_t lru; // 6 bits
} l1d_current_page_entry;

l1d_current_page_entry l1d_current_pages_table[NUM_CPUS][L1D_CURRENT_PAGES_TABLE_ENTRIES];

void l1d_init_current_pages_table() {
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    l1d_current_pages_table[l1d_cpu_id][i].page_addr = 0;
    l1d_current_pages_table[l1d_cpu_id][i].u_vector = 0; // not valid
    for (int j = 0; j < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; j++) {
      l1d_current_pages_table[l1d_cpu_id][i].berti[j] = 0;
    }
    l1d_current_pages_table[l1d_cpu_id][i].current_berti = 0;
    l1d_current_pages_table[l1d_cpu_id][i].stride = 0;
    l1d_current_pages_table[l1d_cpu_id][i].short_reuse = true;
    l1d_current_pages_table[l1d_cpu_id][i].continue_burst = false;
    l1d_current_pages_table[l1d_cpu_id][i].lru = i;
  }
}

uint64_t l1d_get_current_pages_entry(uint64_t page_addr) {
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][i].page_addr == page_addr) return i;
  }
  return L1D_CURRENT_PAGES_TABLE_ENTRIES;
}

void l1d_update_lru_current_pages_table(uint64_t index) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][i].lru < l1d_current_pages_table[l1d_cpu_id][index].lru) { // Found
      l1d_current_pages_table[l1d_cpu_id][i].lru++;
    }
  }
  l1d_current_pages_table[l1d_cpu_id][index].lru = 0;
}

uint64_t l1d_get_lru_current_pages_entry() {
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

void l1d_add_current_pages_table(uint64_t index, uint64_t page_addr) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  l1d_current_pages_table[l1d_cpu_id][index].page_addr = page_addr;
  l1d_current_pages_table[l1d_cpu_id][index].u_vector = 0;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    l1d_current_pages_table[l1d_cpu_id][index].berti[i] = 0;
  }
  l1d_current_pages_table[l1d_cpu_id][index].continue_burst = false;
}

void l1d_update_current_pages_table(uint64_t index, uint64_t offset) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  l1d_current_pages_table[l1d_cpu_id][index].u_vector |= (uint64_t)1 << offset;
  l1d_update_lru_current_pages_table(index);
}

void l1d_remove_offset_current_pages_table(uint64_t index, uint64_t offset) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  l1d_current_pages_table[l1d_cpu_id][index].u_vector &= !((uint64_t)1 << offset);
}

void l1d_add_berti_current_pages_table(uint64_t index, int *berti, unsigned *saved_cycles) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);

  // for each berti collected
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS; i++) {
    if (berti[i] == 0) break;
    //assert(abs(berti[i]) < L1D_PAGE_BLOCKS);
    
    for (int j = 0; j < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; j++) {
      if (l1d_current_pages_table[l1d_cpu_id][index].berti[j] == 0) {
	l1d_current_pages_table[l1d_cpu_id][index].berti[j] = berti[i];
#ifdef BERTI_LATENCIES
	l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] = saved_cycles[i];
#else
	l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] = 1;
#endif
	break;
      } else if (l1d_current_pages_table[l1d_cpu_id][index].berti[j] == berti[i]) {
#ifdef BERTI_LATENCIES
	l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] += saved_cycles[i];
#else
	l1d_current_pages_table[l1d_cpu_id][index].berti_score[j]++;
	//assert(l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] < L1D_PAGE_BLOCKS);
#endif
#ifdef WARMUP_NEW_PAGES
	// For first time accessed pages. No wait until it is evicted to predict
	if (l1d_current_pages_table[l1d_cpu_id][index].current_berti == 0
	    && l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] > 2) {
	  l1d_current_pages_table[l1d_cpu_id][index].current_berti = berti[i];
	}
#endif
	break;
      }
    }
  }
  l1d_update_lru_current_pages_table(index);
}

void l1d_sub_berti_current_pages_table(uint64_t index, int distance) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);

  // for each berti 
  for (int j = distance ; j < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; j++) {
    if (l1d_current_pages_table[l1d_cpu_id][index].berti[j] == 0) {
      break;
    }
#ifdef BERTI_LATENCIES
    if (l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] >= 100) {
      l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] -= 100;
    }
#else
    if (l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] > 0) {
      l1d_current_pages_table[l1d_cpu_id][index].berti_score[j]--;
    }	
#endif
  }
}

int l1d_get_berti_current_pages_table(uint64_t index) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  uint64_t vector = l1d_current_pages_table[l1d_cpu_id][index].u_vector;
  int max_score = 0;
  uint64_t berti = 0;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    int curr_berti = l1d_current_pages_table[l1d_cpu_id][index].berti[i];
    if (curr_berti != 0) { 
      // For every miss reduce next level access latency
      int score = l1d_current_pages_table[l1d_cpu_id][index].berti_score[i];
#if defined(BERTI_LATENCIES) || defined(JUST_BERTI)
      int neg_score = 0; //l1d_count_wrong_berti_bit_vector(vector, curr_berti) * LLC_LATENCY;
#else 
      int neg_score = 0 - abs(curr_berti);
	// ((abs(curr_berti) >> 1) + (abs(curr_berti) >> 2));
      //l1d_count_wrong_berti_bit_vector(vector, curr_berti) - l1d_count_lost_berti_bit_vector(vector, curr_berti);
#endif
      // Modify score based on bad prefetches
      if (score < neg_score) {
	score = 0;
      } else { 
	score -= neg_score;
      }
      if (score >= max_score) { // In case of a draw we choose the larger, since we have bursts 
	berti = curr_berti;
	max_score = score;
      }
    }
  }
  return berti;
}

bool l1d_offset_requested_current_pages_table(uint64_t index, uint64_t offset) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  assert(offset < L1D_PAGE_BLOCKS);
  return l1d_current_pages_table[l1d_cpu_id][index].u_vector & ((uint64_t)1 << offset);
}

void l1d_print_current_pages_table() {
  cout << "CURRENT PAGES: ";
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_current_pages_table[l1d_cpu_id][i].page_addr) {
      cout << "[" << hex << l1d_current_pages_table[l1d_cpu_id][i].page_addr << dec << " ";
      l1d_print_bit_vector(l1d_current_pages_table[l1d_cpu_id][i].u_vector);
      cout << " berti (";
      for (int j = 0; j < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; j++) {
	if (l1d_current_pages_table[l1d_cpu_id][i].berti[j] != 0) {
	  cout << l1d_current_pages_table[l1d_cpu_id][i].berti[j] << "->" << l1d_current_pages_table[l1d_cpu_id][i].berti_score[j] << ",";
	}
      }
      cout << ") ";
      cout << l1d_current_pages_table[l1d_cpu_id][i].lru << "]";
    }
  }
  cout << endl;
}

void l1d_print_current_pages_table(uint64_t index) {
  cout << index << " -> ";
  // if (l1d_current_pages_table[l1d_cpu_id][index].page_addr) {
    cout << "[" << hex << l1d_current_pages_table[l1d_cpu_id][index].page_addr << dec << " ";
    l1d_print_bit_vector(l1d_current_pages_table[l1d_cpu_id][index].u_vector);
    cout << " berti (";
    for (int j = 0; j < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; j++) {
      if (l1d_current_pages_table[l1d_cpu_id][index].berti[j] != 0) {
	cout << l1d_current_pages_table[l1d_cpu_id][index].berti[j] << "->" << l1d_current_pages_table[l1d_cpu_id][index].berti_score[j] << "-" << l1d_count_wrong_berti_bit_vector(l1d_current_pages_table[l1d_cpu_id][index].u_vector, l1d_current_pages_table[l1d_cpu_id][index].berti[j]) << "+" << l1d_count_lost_berti_bit_vector(l1d_current_pages_table[l1d_cpu_id][index].u_vector, l1d_current_pages_table[l1d_cpu_id][index].berti[j]) << ",";
      }
    }
    cout << ") ";
    cout << l1d_current_pages_table[l1d_cpu_id][index].lru << "]";
    //}
  cout << endl;
}

// PREVIOUS REQUESTS TABLE

#define L1D_PREV_REQUESTS_TABLE_INDEX_BITS 10
#define L1D_PREV_REQUESTS_TABLE_ENTRIES (1 << L1D_PREV_REQUESTS_TABLE_INDEX_BITS)
#define L1D_PREV_REQUESTS_TABLE_MASK (L1D_PREV_REQUESTS_TABLE_ENTRIES - 1)
#define L1D_PREV_REQUESTS_TABLE_NULL_POINTER L1D_CURRENT_PAGES_TABLE_ENTRIES

typedef struct __l1d_prev_request_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset; // 6 bits
  uint64_t time; // 16 bits
} l1d_prev_request_entry;

l1d_prev_request_entry l1d_prev_requests_table[NUM_CPUS][L1D_PREV_REQUESTS_TABLE_ENTRIES];
uint64_t l1d_prev_requests_table_head[NUM_CPUS];

void l1d_init_prev_requests_table() {
  l1d_prev_requests_table_head[l1d_cpu_id] = 0;
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_REQUESTS_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_find_prev_request_entry(uint64_t pointer, uint64_t offset) {
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer
	&& l1d_prev_requests_table[l1d_cpu_id][i].offset == offset) return i;
  }
  return L1D_PREV_REQUESTS_TABLE_ENTRIES;
}

void l1d_add_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  // First find for coalescing
  if (l1d_find_prev_request_entry(pointer, offset) != L1D_PREV_REQUESTS_TABLE_ENTRIES) return;

  // Allocate a new entry (evict old one if necessary)
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].page_addr_pointer = pointer;
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].offset = offset;
  l1d_prev_requests_table[l1d_cpu_id][l1d_prev_requests_table_head[l1d_cpu_id]].time = cycle & L1D_TIME_MASK;
  l1d_prev_requests_table_head[l1d_cpu_id] = (l1d_prev_requests_table_head[l1d_cpu_id] + 1) & L1D_PREV_REQUESTS_TABLE_MASK;
}

void l1d_reset_pointer_prev_requests(uint64_t pointer) {
  for (int i = 0; i < L1D_PREV_REQUESTS_TABLE_ENTRIES; i++) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
      l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer = L1D_PREV_REQUESTS_TABLE_NULL_POINTER;
    }
  }
}

void l1d_print_prev_requests_table() {
  cout << "PREV REQUESTS: ";
  for (uint64_t i = (l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK; i != l1d_prev_requests_table_head[l1d_cpu_id]; i = (i + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer != L1D_PREV_REQUESTS_TABLE_NULL_POINTER) {
    cout << "[" << l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer << " " << l1d_prev_requests_table[l1d_cpu_id][i].offset << " @" << l1d_prev_requests_table[l1d_cpu_id][i].time << "] ";
    }  
  }
  cout << endl;
}

void l1d_print_prev_requests_table(uint64_t pointer) {
  cout << "PREV REQUESTS: ";
  for (uint64_t i = (l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK; i != l1d_prev_requests_table_head[l1d_cpu_id]; i = (i + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK) {
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
    cout << "[" << l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer << " " << l1d_prev_requests_table[l1d_cpu_id][i].offset << " @" << l1d_prev_requests_table[l1d_cpu_id][i].time << "] ";
    }  
  }
  cout << endl;
}

// req_time is 0 if already requested (fill) or current time if (hit)
void l1d_get_berti_prev_requests_table(uint64_t pointer, uint64_t offset, uint64_t latency, int *berti, unsigned *saved_cycles, uint64_t req_time) {
  int my_pos = 0;
  uint64_t extra_time = 0;
  uint64_t last_time = l1d_prev_requests_table[l1d_cpu_id][(l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK].time;
  //cout << "Latency " << latency << " " << pointer << " " << offset << " ";
  //l1d_print_prev_requests_table(pointer);
  for (uint64_t i = (l1d_prev_requests_table_head[l1d_cpu_id] + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK; i != l1d_prev_requests_table_head[l1d_cpu_id]; i = (i + L1D_PREV_REQUESTS_TABLE_MASK) & L1D_PREV_REQUESTS_TABLE_MASK) {
    // Against the time overflow
    if (last_time < l1d_prev_requests_table[l1d_cpu_id][i].time) {
      extra_time = L1D_TIME_OVERFLOW;
    }
    last_time = l1d_prev_requests_table[l1d_cpu_id][i].time;  
    if (l1d_prev_requests_table[l1d_cpu_id][i].page_addr_pointer == pointer) { // Same page
      if (l1d_prev_requests_table[l1d_cpu_id][i].offset == offset) { // Its me
	req_time = l1d_prev_requests_table[l1d_cpu_id][i].time;
      } else if (req_time) { // Not me (check only older than me)
	if (l1d_prev_requests_table[l1d_cpu_id][i].time <= req_time + extra_time - latency) {
	  berti[my_pos] = l1d_calculate_stride(l1d_prev_requests_table[l1d_cpu_id][i].offset, offset);
	  saved_cycles[my_pos] = latency;
	  //cout << "pos1 " << my_pos << ": " << berti[my_pos] << "->" << saved_cycles[my_pos] << endl;
	  my_pos++;
	} else if (req_time + extra_time - l1d_prev_requests_table[l1d_cpu_id][i].time > 0) { // Only if some savings
#ifdef BERTI_LATENCIES
	  berti[my_pos] = l1d_calculate_stride(l1d_prev_requests_table[l1d_cpu_id][i].offset, offset);
	  saved_cycles[my_pos] = req_time + extra_time - l1d_prev_requests_table[l1d_cpu_id][i].time;
	  //cout << "pos2 " << my_pos << ": " << berti[my_pos] << "->" << saved_cycles[my_pos] << " " << extra_time << " " << l1d_prev_requests_table[l1d_cpu_id][i].time << endl;
	  my_pos++;
#endif
	}
	if (my_pos == L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS) {
	  berti[my_pos] = 0;
	  return;
	}
      }
    }
  }
  berti[my_pos] = 0;
}

// LATENCIES TABLE

#define L1D_LATENCIES_TABLE_INDEX_BITS 10
#define L1D_LATENCIES_TABLE_ENTRIES (1 << L1D_LATENCIES_TABLE_INDEX_BITS)
#define L1D_LATENCIES_TABLE_MASK (L1D_LATENCIES_TABLE_ENTRIES - 1)
#define L1D_LATENCIES_TABLE_NULL_POINTER L1D_CURRENT_PAGES_TABLE_ENTRIES

// We do not have access to the MSHR, so we aproximate it using this structure.
typedef struct __l1d_latency_entry {
  uint64_t page_addr_pointer; // 6 bits
  uint64_t offset; // 6 bits
  uint64_t time_lat; // 16 bits // time if not completed, latency if completed
  bool completed; // 1 bit
} l1d_latency_entry;

l1d_latency_entry l1d_latencies_table[NUM_CPUS][L1D_LATENCIES_TABLE_ENTRIES];
uint64_t l1d_latencies_table_head[NUM_CPUS];

void l1d_init_latencies_table() {
  l1d_latencies_table_head[l1d_cpu_id] = 0;
  for (int i = 0; i < L1D_LATENCIES_TABLE_ENTRIES; i++) {
    l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer = L1D_LATENCIES_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_find_latency_entry(uint64_t pointer, uint64_t offset) {
  for (int i = 0; i < L1D_LATENCIES_TABLE_ENTRIES; i++) {
    if (l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer == pointer
	&& l1d_latencies_table[l1d_cpu_id][i].offset == offset) return i;
  }
  return L1D_LATENCIES_TABLE_ENTRIES;
}

void l1d_add_latencies_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  // First find for coalescing
  if (l1d_find_latency_entry(pointer, offset) != L1D_LATENCIES_TABLE_ENTRIES) return;

  // Allocate a new entry (evict old one if necessary)
  l1d_latencies_table[l1d_cpu_id][l1d_latencies_table_head[l1d_cpu_id]].page_addr_pointer = pointer;
  l1d_latencies_table[l1d_cpu_id][l1d_latencies_table_head[l1d_cpu_id]].offset = offset;
  l1d_latencies_table[l1d_cpu_id][l1d_latencies_table_head[l1d_cpu_id]].time_lat = cycle & L1D_TIME_MASK;
  l1d_latencies_table[l1d_cpu_id][l1d_latencies_table_head[l1d_cpu_id]].completed = false;
  l1d_latencies_table_head[l1d_cpu_id] = (l1d_latencies_table_head[l1d_cpu_id] + 1) & L1D_LATENCIES_TABLE_MASK;
}

void l1d_reset_pointer_latencies(uint64_t pointer) {
  for (int i = 0; i < L1D_LATENCIES_TABLE_ENTRIES; i++) {
    if (l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
      l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer = L1D_LATENCIES_TABLE_NULL_POINTER;
    }
  }
}

void l1d_reset_entry_latencies_table(uint64_t pointer, uint64_t offset) {
  uint64_t index = l1d_find_latency_entry(pointer, offset);
  if (index != L1D_LATENCIES_TABLE_ENTRIES) {
    l1d_latencies_table[l1d_cpu_id][index].page_addr_pointer = L1D_LATENCIES_TABLE_NULL_POINTER;
  }
}

uint64_t l1d_get_and_set_latency_latencies_table(uint64_t pointer, uint64_t offset, uint64_t cycle) {
  uint64_t index = l1d_find_latency_entry(pointer, offset); 
  if (index == L1D_LATENCIES_TABLE_ENTRIES) return 0;
  if (!l1d_latencies_table[l1d_cpu_id][index].completed) {
    l1d_latencies_table[l1d_cpu_id][index].time_lat = l1d_get_latency(cycle, l1d_latencies_table[l1d_cpu_id][index].time_lat);
    l1d_latencies_table[l1d_cpu_id][index].completed = true;
  }    
  return l1d_latencies_table[l1d_cpu_id][index].time_lat;
}

uint64_t l1d_get_latency_latencies_table(uint64_t pointer, uint64_t offset) {
  uint64_t index = l1d_find_latency_entry(pointer, offset);
  if (index == L1D_LATENCIES_TABLE_ENTRIES) return 0;
  if (!l1d_latencies_table[l1d_cpu_id][index].completed) return 0;
  return l1d_latencies_table[l1d_cpu_id][index].time_lat;
}

bool l1d_ongoing_request(uint64_t pointer, uint64_t offset) {
  uint64_t index = l1d_find_latency_entry(pointer, offset);
  if (index == L1D_LATENCIES_TABLE_ENTRIES) return false;
  if (l1d_latencies_table[l1d_cpu_id][index].completed) return false;
  return true;
}

bool l1d_is_request(uint64_t pointer, uint64_t offset) {
  uint64_t index = l1d_find_latency_entry(pointer, offset);
  if (index == L1D_LATENCIES_TABLE_ENTRIES) return false;
  return true;
}

void l1d_print_latencies_table() {
  cout << "LATENCIES: ";
  for (int i = 0; i < L1D_LATENCIES_TABLE_ENTRIES; i++) {
    if (l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer != L1D_LATENCIES_TABLE_NULL_POINTER) {
    cout << "[" << l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer << " " << l1d_latencies_table[l1d_cpu_id][i].offset << " @" << l1d_latencies_table[l1d_cpu_id][i].time_lat << "] ";
    }  
  }
  cout << endl;
}

void l1d_print_latencies_table(uint64_t pointer) {
  cout << "LATENCIES: ";
  for (int i = 0; i < L1D_LATENCIES_TABLE_ENTRIES; i++) {
    if (l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer == pointer) {
    cout << "[" << l1d_latencies_table[l1d_cpu_id][i].page_addr_pointer << " " << l1d_latencies_table[l1d_cpu_id][i].offset << " @" << l1d_latencies_table[l1d_cpu_id][i].time_lat << "] ";
    }  
  }
  cout << endl;
}


// RECORD PAGES TABLE

#define L1D_RECORD_PAGES_TABLE_INDEX_BITS 14
#define L1D_RECORD_PAGES_TABLE_ENTRIES ((1 << L1D_RECORD_PAGES_TABLE_INDEX_BITS) - 1) // Null pointer for ip table
#define L1D_TRUNCATED_PAGE_ADDR_BITS 32 // 4 bytes
#define L1D_TRUNCATED_PAGE_ADDR_MASK (((uint64_t)1 << L1D_TRUNCATED_PAGE_ADDR_BITS) -1)

typedef struct __l1d_record_page_entry {
  uint64_t page_addr; // 4 bytes
  uint64_t linnea; // 8 bytes
  uint64_t last_offset; // 6 bits
  bool short_reuse; // 1 bit
  uint64_t lru; // 10 bits
} l1d_record_page_entry;

l1d_record_page_entry l1d_record_pages_table[NUM_CPUS][L1D_RECORD_PAGES_TABLE_ENTRIES];

void l1d_init_record_pages_table() {
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    l1d_record_pages_table[l1d_cpu_id][i].page_addr = 0;
    l1d_record_pages_table[l1d_cpu_id][i].linnea = 0;
    l1d_record_pages_table[l1d_cpu_id][i].last_offset = 0;
    l1d_record_pages_table[l1d_cpu_id][i].short_reuse = true;
    l1d_record_pages_table[l1d_cpu_id][i].lru = i;
  }
}

uint64_t l1d_get_lru_record_pages_entry() {
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

void l1d_update_lru_record_pages_table(uint64_t index) {
  assert(index < L1D_RECORD_PAGES_TABLE_ENTRIES);
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].lru < l1d_record_pages_table[l1d_cpu_id][index].lru) { // Found
      l1d_record_pages_table[l1d_cpu_id][i].lru++;
    }
  }
  l1d_record_pages_table[l1d_cpu_id][index].lru = 0;
}

uint64_t l1d_get_entry_record_pages_table(uint64_t page_addr) {
  uint64_t trunc_page_addr = page_addr & L1D_TRUNCATED_PAGE_ADDR_MASK;  
  for (int i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].page_addr == trunc_page_addr) { // Found
      return i;
    }
  }
  return L1D_RECORD_PAGES_TABLE_ENTRIES;
}

void l1d_add_record_pages_table(uint64_t page_addr, uint64_t new_page_addr, uint64_t last_offset = 0, bool short_reuse = true) {
  uint64_t index = l1d_get_entry_record_pages_table(page_addr);
  if (index < L1D_RECORD_PAGES_TABLE_ENTRIES) {
    l1d_update_lru_record_pages_table(index);
  } else {
    index = l1d_get_lru_record_pages_entry();
    l1d_record_pages_table[l1d_cpu_id][index].page_addr = page_addr & L1D_TRUNCATED_PAGE_ADDR_MASK;
  }
  l1d_record_pages_table[l1d_cpu_id][index].linnea = new_page_addr;
  l1d_record_pages_table[l1d_cpu_id][index].last_offset = last_offset;
  l1d_record_pages_table[l1d_cpu_id][index].short_reuse = short_reuse;
}

void l1d_print_record_pages_table() {
  cout << "RECORD PAGES: ";
  for (uint64_t i = 0; i < L1D_RECORD_PAGES_TABLE_ENTRIES; i++) {
    if (l1d_record_pages_table[l1d_cpu_id][i].page_addr) {
      cout << "[";
      cout << hex << l1d_record_pages_table[l1d_cpu_id][i].page_addr << dec << " ";
      cout << l1d_record_pages_table[l1d_cpu_id][i].linnea << " ";
      cout << l1d_record_pages_table[l1d_cpu_id][i].last_offset << "] ";
    }
  }
  cout << endl;
}

void l1d_print_record_pages_table(uint64_t index) {
  cout << "RECORD PAGE: [";
  cout << hex << l1d_record_pages_table[l1d_cpu_id][index].page_addr << dec << " ";
  cout << l1d_record_pages_table[l1d_cpu_id][index].linnea << " ";
  cout << l1d_record_pages_table[l1d_cpu_id][index].last_offset << "]" << endl;
}


// IP TABLE

#define L1D_IP_TABLE_INDEX_BITS 12
#define L1D_IP_TABLE_ENTRIES (1 << L1D_IP_TABLE_INDEX_BITS)
#define L1D_IP_TABLE_INDEX_MASK (L1D_IP_TABLE_ENTRIES - 1)

typedef struct __l1d_ip_entry {
  bool current; // 1 bit
  int berti_or_pointer; // 7 bits // Berti if current == 0
  bool consecutive; // 1 bit
  bool short_reuse; // 1 bit
} l1d_ip_entry;

l1d_ip_entry l1d_ip_table[NUM_CPUS][L1D_IP_TABLE_ENTRIES];

//Stats
uint64_t l1d_ip_misses[NUM_CPUS][L1D_IP_TABLE_ENTRIES];
uint64_t l1d_ip_hits[NUM_CPUS][L1D_IP_TABLE_ENTRIES];
uint64_t l1d_ip_late[NUM_CPUS][L1D_IP_TABLE_ENTRIES];
uint64_t l1d_ip_early[NUM_CPUS][L1D_IP_TABLE_ENTRIES];
uint64_t l1d_stats_pref_addr;
uint64_t l1d_stats_pref_ip;
uint64_t l1d_stats_pref_current;
uint64_t cache_accesses;
uint64_t cache_misses;

void l1d_init_ip_table() {
  for (int i = 0; i < L1D_IP_TABLE_ENTRIES; i++) {
    l1d_ip_table[l1d_cpu_id][i].current = false;
    l1d_ip_table[l1d_cpu_id][i].berti_or_pointer = 0;
    l1d_ip_table[l1d_cpu_id][i].consecutive = false;
    l1d_ip_table[l1d_cpu_id][i].short_reuse = true;
    
    l1d_ip_misses[l1d_cpu_id][i] = 0;
    l1d_ip_hits[l1d_cpu_id][i] = 0;
    l1d_ip_late[l1d_cpu_id][i] = 0;
    l1d_ip_early[l1d_cpu_id][i] = 0;
  }
  l1d_stats_pref_addr = 0;
  l1d_stats_pref_ip = 0;
  l1d_stats_pref_current = 0;
  cache_accesses = 0;
  cache_misses = 0;
}

void l1d_update_ip_table(int pointer, int berti, int stride, bool short_reuse) {
  for (int i = 0; i < L1D_IP_TABLE_ENTRIES; i++) {
    if (l1d_ip_table[l1d_cpu_id][i].current
	&& l1d_ip_table[l1d_cpu_id][i].berti_or_pointer == pointer) {
      l1d_ip_table[l1d_cpu_id][i].current = false;
      if (short_reuse) {
	l1d_ip_table[l1d_cpu_id][i].berti_or_pointer = berti;
      } else {
	l1d_ip_table[l1d_cpu_id][i].berti_or_pointer = stride;
      }
      l1d_ip_table[l1d_cpu_id][i].short_reuse = short_reuse;
    }
  }
}

void l1d_print_ip_table() {
  cout << "IP: ";
  for (int i = 0; i < L1D_IP_TABLE_ENTRIES; i++) {
    cout << "[" << hex << i << " -> " << dec << l1d_ip_table[l1d_cpu_id][i].current
	 << " " << l1d_ip_table[l1d_cpu_id][i].berti_or_pointer << "] ";
  }
  cout << endl;
}

void l1d_print_ip_table(uint64_t index) {
  assert(index < L1D_IP_TABLE_ENTRIES);
  cout << "IP: [" << hex << index << " -> " << dec << l1d_ip_table[l1d_cpu_id][index].current
       << " " << l1d_ip_table[l1d_cpu_id][index].berti_or_pointer << "] " << endl;
}

// INTERTABLES

uint64_t l1d_evict_lru_current_page_entry() {
  // Find victim and clear pointers to it
  uint64_t victim_index = l1d_get_lru_current_pages_entry(); // already updates lru
  assert(victim_index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  
  // From all timely delta found, we record the best 
  if (l1d_current_pages_table[l1d_cpu_id][victim_index].u_vector) { // Accessed entry
    
    // Update any IP pointing to it
    l1d_update_ip_table(victim_index,
			l1d_get_berti_current_pages_table(victim_index),
			l1d_current_pages_table[l1d_cpu_id][victim_index].stride,
			l1d_current_pages_table[l1d_cpu_id][victim_index].short_reuse);
  }
  
  l1d_reset_pointer_prev_requests(victim_index); // Not valid anymore
  l1d_reset_pointer_latencies(victim_index); // Not valid anymore
  
  return victim_index;
}

void l1d_evict_current_page_entry(uint64_t index) {
  assert(index < L1D_CURRENT_PAGES_TABLE_ENTRIES);
  
  // From all timely delta found, we record the best 
  if (l1d_current_pages_table[l1d_cpu_id][index].u_vector) { // Accessed entry
    
    // Update any IP pointing to it
    l1d_update_ip_table(index,
			l1d_get_berti_current_pages_table(index),
			l1d_current_pages_table[l1d_cpu_id][index].stride,
			l1d_current_pages_table[l1d_cpu_id][index].short_reuse);
  }
  
  l1d_reset_pointer_prev_requests(index); // Not valid anymore
  l1d_reset_pointer_latencies(index); // Not valid anymore
}

void l1d_remove_current_table_entry(uint64_t index) {
  l1d_current_pages_table[l1d_cpu_id][index].page_addr = 0;
  l1d_current_pages_table[l1d_cpu_id][index].u_vector = 0;
  for (int i = 0; i < L1D_CURRENT_PAGES_TABLE_NUM_BERTI; i++) {
    l1d_current_pages_table[l1d_cpu_id][index].berti[i] = 0;
  }
}

// INTERFACE

void CACHE::prefetcher_initialize() 
{
  // get_pq_size().back() = 16;
  
  l1d_cpu_id = cpu;
  cout << "CPU " << cpu << " L1D Berti prefetcher" << endl;
  
  l1d_init_current_pages_table();
  l1d_init_prev_requests_table();
  l1d_init_latencies_table();
  l1d_init_record_pages_table();
  l1d_init_ip_table();

}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint64_t instr_id, uint8_t cache_hit, bool prefetch_hit, uint8_t type, uint8_t ld_type, uint32_t metadata_in)
{
  assert((access_type) type == access_type::LOAD || (access_type) type == access_type::RFO);

  cache_accesses++;
  if (!cache_hit) cache_misses++;
  
  l1d_cpu_id = cpu;
  uint64_t line_addr = addr >> LOG2_BLOCK_SIZE;
  uint64_t page_addr = line_addr >> L1D_PAGE_BLOCKS_BITS;
  uint64_t offset = line_addr & L1D_PAGE_OFFSET_MASK;
  uint64_t ip_index = ip & L1D_IP_TABLE_INDEX_MASK;
  
  int last_berti = 0;
  int berti = 0;
  bool linnea_hits = false;
  bool first_access = false;
  bool full_access = false;
  int stride = 0;
  bool short_reuse = true;
  uint64_t count_reuse = 0;
  
  // Find the entry in the current page table
  uint64_t index = l1d_get_current_pages_entry(page_addr);
  
  bool recently_accessed = false;
  if (index < L1D_CURRENT_PAGES_TABLE_ENTRIES) { // Hit in current page table
    recently_accessed = l1d_offset_requested_current_pages_table(index, offset);
  }
  
  if (index < L1D_CURRENT_PAGES_TABLE_ENTRIES  // Hit in current page table
      && l1d_current_pages_table[cpu][index].u_vector != 0) { // Used before
    
    // Within the same page we always predict the same
    last_berti = l1d_current_pages_table[cpu][index].current_berti;
    berti = last_berti;
    
    // Update accessed block vector
    l1d_update_current_pages_table(index, offset);
    
    // Update berti
    if (cache_hit) { // missed update it when resolved
      uint64_t latency = l1d_get_latency_latencies_table(index, offset);
      if (latency != 0) {
	// Find berti distance from pref_latency cycles before
	int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS]; 
	unsigned saved_cycles[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS]; 
	//cout << "Hit ";
	l1d_get_berti_prev_requests_table(index, offset, latency, berti, saved_cycles, current_cycle);
	/* if (ip_index == 0x10f) { */
	/*   cout << "ADD BERTI HIT " << index << " <" << offset << "> "; */
	/*   for (int i = 0; berti[i] != 0; i++) { */
	/*     cout << berti[i] << " " << saved_cycles[i] << ", "; */
	/*   } */
	/*   cout << endl; */
	/* } */

	if (!recently_accessed) { // If not accessed recently
	  l1d_add_berti_current_pages_table(index, berti, saved_cycles);
	}	
	// Eliminate a prev prefetch since it has been used // Better do it on evict
	//l1d_reset_entry_latencies_table(index, offset);
      }
    }
    
  } else { // First access to a new page
    
    first_access = true;
    
    // Find Berti and Linnea
    
    // Check IP table
    if (l1d_ip_table[cpu][ip_index].current) { // Here we check for Berti and Linnea
      
      int ip_pointer = l1d_ip_table[cpu][ip_index].berti_or_pointer;
      assert(ip_pointer < L1D_CURRENT_PAGES_TABLE_ENTRIES);
      // It will be a change of page for the IP
      
      // Get the last berti the IP is using and new berti to use
      last_berti = l1d_current_pages_table[cpu][ip_pointer].current_berti;
      berti = l1d_get_berti_current_pages_table(ip_pointer);
      
      // Get if all blocks for a potential burst were accessed
      full_access = l1d_all_last_berti_accessed_bit_vector(l1d_current_pages_table[cpu][ip_pointer].u_vector, berti);
      
      // Make the link (linnea)
      uint64_t last_page_addr = l1d_current_pages_table[cpu][ip_pointer].page_addr;
      
      /* if (ip_index == 0x28c) { cout << "LINK " << hex << last_page_addr << " " << page_addr << " " << dec << ip_pointer << " " << l1d_count_bit_vector(l1d_current_pages_table[cpu][ip_pointer].u_vector) << " "; */
      /*   l1d_print_bit_vector(l1d_current_pages_table[cpu][ip_pointer].u_vector);  */
      /*   cout << endl; */
      /* } */

      count_reuse = l1d_count_bit_vector(l1d_current_pages_table[cpu][ip_pointer].u_vector);
      short_reuse = (count_reuse > LONG_REUSE_LIMIT);
      if (short_reuse) {
	if (berti > 0 && last_page_addr + 1 == page_addr) {
	  l1d_ip_table[cpu][ip_index].consecutive = true;
	} else if (berti < 0 && last_page_addr == page_addr + 1) {
	  l1d_ip_table[cpu][ip_index].consecutive = true;
	} else { // Only add to record if not consecutive
	  l1d_ip_table[cpu][ip_index].consecutive = false;
	  l1d_add_record_pages_table(last_page_addr, page_addr);
	}
      } else {
	if (l1d_current_pages_table[cpu][ip_pointer].short_reuse) {
	  l1d_current_pages_table[cpu][ip_pointer].short_reuse = false;
	}
	uint64_t record_index = l1d_get_entry_record_pages_table(last_page_addr);
	//if (ip_index == 0x28c) cout << "LONG REUSE SECOND " << hex << last_page_addr << "->" << l1d_get_entry_record_pages_table(last_page_addr) << " " << page_addr << "->" << l1d_get_entry_record_pages_table(page_addr) << dec << endl;
	if (record_index < L1D_RECORD_PAGES_TABLE_ENTRIES
	    && !l1d_record_pages_table[cpu][record_index].short_reuse
	    && l1d_record_pages_table[cpu][record_index].linnea == page_addr) {
	  stride = l1d_calculate_stride(l1d_record_pages_table[cpu][record_index].last_offset, offset);
	  //if (ip_index == 0x28c) cout << "LONG REUSE MATCH " << hex << last_page_addr << " " << page_addr << " " << dec << l1d_record_pages_table[cpu][record_index].last_offset << " " << offset << " " << stride << endl;
	}
	//if (ip_index == 0x28c) cout << "LONG REUSE ADD RECORD " << hex << last_page_addr << " " << page_addr << " " << dec << offset << " " << short_reuse << endl;
	
	if (!recently_accessed) { // If not accessed recently
	  l1d_add_record_pages_table(last_page_addr, page_addr, offset, short_reuse);
	}
	//if (ip_index == 0x28c) cout << "LONG REUSE RECORDED " << hex << last_page_addr << "->" << l1d_get_entry_record_pages_table(last_page_addr) << dec << endl;
      }
      
    } else {
      berti = l1d_ip_table[cpu][ip_index].berti_or_pointer;
    }
    
    if (index == L1D_CURRENT_PAGES_TABLE_ENTRIES) { // Miss in current page table
      
      // Not found (linnea did not work or was not used -- berti == 0)
      
      // Add new page entry evicting a previous one.
      index = l1d_evict_lru_current_page_entry();
      l1d_add_current_pages_table(index, page_addr);
      
    } else { // First access, but linnea worked and blocks of the page have been prefetched
      linnea_hits = true;
    }
    
    // Update accessed block vector
    l1d_update_current_pages_table(index, offset);
    
  }
    
  // Set the new berti
  if (!recently_accessed) { // If not accessed recently
    if (short_reuse) {
      l1d_current_pages_table[cpu][index].current_berti = berti;
    } else {
      l1d_current_pages_table[cpu][index].stride = stride;
    }
    l1d_current_pages_table[cpu][index].short_reuse = short_reuse;
    
    l1d_ip_table[cpu][ip_index].current = true;
    l1d_ip_table[cpu][ip_index].berti_or_pointer = index;
  }

  // Add the request in the history buffer
  if (l1d_find_prev_request_entry(index, offset) == L1D_PREV_REQUESTS_TABLE_ENTRIES) { // Not in prev
    l1d_add_prev_requests_table(index, offset, current_cycle);
  } else {
    if (!cache_hit && !l1d_ongoing_request(index, offset)) {
      l1d_add_prev_requests_table(index, offset, current_cycle);      
    }
  }
  
  // Add miss in the latency table
  if (!recently_accessed && !cache_hit) { // If not accessed recently
    //l1d_reset_entry_latencies_table(index, offset); // If completed, add a new one
    l1d_add_latencies_table(index, offset, current_cycle);
  }

  if (cache_hit) {
    l1d_ip_hits[cpu][ip_index]++;
  } else if (l1d_ongoing_request(index, offset)) {
    l1d_ip_late[cpu][ip_index]++;
  } else if (l1d_is_request(index, offset)) {
    l1d_ip_early[cpu][ip_index]++;
  } else {
    l1d_ip_misses[cpu][ip_index]++;
  }
  
  /* if (page_addr == 0x8d9869085) { */
  /*   cout << "IP: " << hex << ip_index << dec << " " << offset << endl; */
  /* } */
  
  /* if (ip_index == 0x475 */
  /*   	/\* || ip_index == 0x30f *\/ */
  /*    	/\* || ip_index == 0x3cb *\/ */
  /*   /\* 	|| ip_index == 0x2be *\/ */
  /*   /\* 	|| ip_index == 0x202 *\/ */
  /*   /\* 	|| ip_index == 0x267 *\/ */
  /*   /\* 	|| ip_index == 0x233 *\/ */
  /*   /\* 	|| ip_index == 0xdb *\/ */
  /*   /\* 	|| ip_index == 0x143 *\/ */
  /*   /\* 	|| ip_index == 0x114 *\/ */
  /*   	) { */
  /*     cout << "@" << current_cycle << hex << " " << addr << " " << line_addr << " " << page_addr << dec << endl; */
  /*     cout << "CURRENT " << hex << (ip_index) << dec << " " << status << " "; */
  /*     l1d_print_current_pages_table(index); */
  /*     //l1d_print_prev_requests_table(); */

  /*     // PREDICT */
  /*     cout << "PREDICTION (" << get_pq_occupancy().back() << "/" << get_pq_size().back() << "): " << hex << page_addr << " " << ip << dec << " " << berti << ": <" << offset << "> " << endl; */
      
  /*     l1d_print_ip_table(ip_index); */
  /*     if (l1d_ip_table[cpu][ip_index].current) { */
  /*   	cout << "Current page berti " << l1d_current_pages_table[cpu][l1d_ip_table[cpu][ip_index].berti_or_pointer].current_berti << endl; */
  /*     } */
      
  /*     if (first_access && berti != 0) { */
  /*   	cout << "FIRST " << linnea_hits << " " << berti << " "; */
  /*   	if (linnea_hits) { */
  /*   	  if (berti > last_berti) { // larger berti: semi burst */
  /*   	    cout << "SEMI BURST" << endl; */
  /*   	  } else { */
  /*   	    cout << endl; */
  /*   	  } */
  /*   	} else { // Linnea missed: full burst */
  /*   	  cout << "FULL BURST " << full_access << endl; */
  /*   	} */
  /*     } */

  /*   } */

  if (berti != 0) {
    
    // Burst mode
    if ((first_access && full_access) || l1d_current_pages_table[cpu][index].continue_burst) {
      int burst_init = 0;
      int burst_end = 0;
      int burst_it = 0;
      if (!linnea_hits || l1d_current_pages_table[cpu][index].continue_burst) { // Linnea missed: full burst
	l1d_current_pages_table[cpu][index].continue_burst = false;
	if (berti > 0) {
	  burst_init = offset + 1;
	  burst_end = offset + berti;
	  burst_it = 1;
	} else {
	  burst_init = offset - 1;
	  burst_end = offset + berti;
	  burst_it = -1;
	}
      } else if (last_berti > 0 && berti > 0 && berti > last_berti) { // larger abs berti: semi burst
	burst_init = last_berti;
	burst_end = berti;
	burst_it = 1;
	} else if (last_berti < 0 && berti < 0 && berti < last_berti) { // larger abs berti: semi burst
	burst_init = L1D_PAGE_OFFSET_MASK + last_berti;
	burst_end = L1D_PAGE_OFFSET_MASK + berti;
	burst_it = -1;
      }
      int bursts = 0;
      //if (ip_index == 0x10f) cout << "BURST " << burst_init << " " << burst_end << endl;
      for (int i = burst_init; i != burst_end; i += burst_it) {
	//if (i < 0 || i >= L1D_PAGE_BLOCKS) cout << i << " " << burst_init << " " << burst_end << " " << burst_it << endl;
	if (i >= 0 && i < L1D_PAGE_BLOCKS) { // Burst are for the current page
	  uint64_t pf_line_addr = (page_addr << L1D_PAGE_BLOCKS_BITS) | i;
	  uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
	  uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
	  // We are doing the berti here. Do not leave space for it
	  if (get_pq_occupancy().back() < get_pq_size().back() && bursts < L1D_BURST_THROTTLING) { 
	    //if (ip_index == 0x10f) cout << "BURST PREFETCH " << hex << page_addr << dec << " <" << pf_offset << ">" << endl;
	    bool prefetched = prefetch_line(pf_addr, true, (LOAD_TYPE) ld_type, 1);
	    assert(prefetched);
	    l1d_add_latencies_table(index, pf_offset, current_cycle);
	    bursts++;
	  } else { // record last burst
#ifdef CONTINUE_BURST
	    if (!recently_accessed) { // If not accessed recently
	      l1d_current_pages_table[cpu][index].continue_burst = true;
	    }
#endif
	    break;
	  }
	}
      }
    }
    
    // Berti mode
    for (int i = 1; i <= L1D_BERTI_THROTTLING; i++) {
      
      // If the prefetcher will be done
      if (get_pq_occupancy().back() < get_pq_size().back()) {
	
	uint64_t pf_line_addr = line_addr + (berti * i);
	uint64_t pf_addr = pf_line_addr << LOG2_BLOCK_SIZE;
	uint64_t pf_page_addr = pf_line_addr >> L1D_PAGE_BLOCKS_BITS;
	uint64_t pf_offset = pf_line_addr & L1D_PAGE_OFFSET_MASK;
	
	// Same page, prefetch standard
	if (pf_page_addr == page_addr) { 
	  //if (ip_index == 0x10f) cout << "BERTI PREFETCH " << hex << page_addr << dec << " <" << pf_offset << ">" << endl;
	  bool prefetched = prefetch_line(pf_addr, true, (LOAD_TYPE) ld_type, 1);
	  assert(prefetched);
	  l1d_add_latencies_table(index, pf_offset, current_cycle);
	  
	  // Out of page, try consecutive first
	} else if (l1d_ip_table[cpu][ip_index].consecutive && berti != 0) { 
	  uint64_t new_page;
	  if (berti < 0) {
	    new_page = page_addr - 1;
	  } else {
	    new_page = page_addr + 1;
	  }
	  
	  // Need to add the linnea page to current pages
	  uint64_t new_index = l1d_get_current_pages_entry(new_page);
	  
	  if (new_index == L1D_CURRENT_PAGES_TABLE_ENTRIES) {
	    
	    // Add new page entry evicting a previous one.
	    new_index = l1d_evict_lru_current_page_entry();
	    l1d_add_current_pages_table(new_index, new_page);
	    
	  }
	  
	  uint64_t pf_offset = (offset + berti + L1D_PAGE_BLOCKS) & L1D_PAGE_OFFSET_MASK;
	  uint64_t new_line = new_page << L1D_PAGE_BLOCKS_BITS;
	  uint64_t new_pf_line = new_line | pf_offset;
	  uint64_t new_addr = new_line << LOG2_BLOCK_SIZE;
	  uint64_t new_pf_addr = new_pf_line << LOG2_BLOCK_SIZE;
	  
	  //cout << "CONSECUTIVE " << hex << new_page << " " << dec << pf_offset << hex << " " << " " << new_line << " " << new_pf_line << " " << new_addr << " " << new_pf_addr << dec << endl;
	  
	  //if (ip_index == 0x10f) cout << "CONSECUTIVE PREFETCH " << hex << new_page << dec << " <" << pf_offset << ">" << endl;
	  bool prefetched = prefetch_line(new_pf_addr, true, (LOAD_TYPE) ld_type, 1);
	  assert(prefetched);
	  l1d_add_latencies_table(new_index, pf_offset, current_cycle);
	  
	} else { // Out of page, try Linnea
#ifdef LINNEA
	  uint64_t index_record = l1d_get_entry_record_pages_table(page_addr);
	  if (index_record < L1D_RECORD_PAGES_TABLE_ENTRIES) { // Linnea found
	    
	    uint64_t new_page = l1d_record_pages_table[cpu][index_record].linnea;
	    
	    // Need to add the linnea page to current pages
	    uint64_t new_index = l1d_get_current_pages_entry(new_page);
	      
	    if (new_index == L1D_CURRENT_PAGES_TABLE_ENTRIES) {
		
	      // Add new page entry evicting a previous one.
	      new_index = l1d_evict_lru_current_page_entry();
	      l1d_add_current_pages_table(new_index, new_page);
		
	    }
	      
	    uint64_t pf_offset = (offset + berti + L1D_PAGE_BLOCKS) & L1D_PAGE_OFFSET_MASK;
	    uint64_t new_line = new_page << L1D_PAGE_BLOCKS_BITS;
	    uint64_t new_pf_line = new_line | pf_offset;
	    uint64_t new_addr = new_line << LOG2_BLOCK_SIZE;
	    uint64_t new_pf_addr = new_pf_line << LOG2_BLOCK_SIZE;
	      
	    //cout << "LINNEA " << hex << new_page << " " << dec << pf_offset << hex << " " << " " << new_line << " " << new_pf_line << " " << new_addr << " " << new_pf_addr << dec << endl;

	    //if (ip_index == 0x10f) cout << "LINNEA PREFETCH " << hex << new_page << dec << " <" << pf_offset << ">" << endl;
	    bool prefetched = prefetch_line(new_pf_addr, true, (LOAD_TYPE) ld_type, 1);
	    assert(prefetched);
	    l1d_add_latencies_table(new_index, pf_offset, current_cycle);
	  }
#endif
	}
      }
    }
  }

#ifdef PREFETCH_FOR_LONG_REUSE
  if (!short_reuse) { // Use stride as it is a long reuse ip
    
    assert(!l1d_ip_table[cpu][ip_index].short_reuse || !l1d_current_pages_table[cpu][index].short_reuse);
      
    // If the prefetcher will be done
    if (get_pq_occupancy().back() < get_pq_size().back()) {
	
      uint64_t index_record = l1d_get_entry_record_pages_table(page_addr);
      if (index_record < L1D_RECORD_PAGES_TABLE_ENTRIES) { // Linnea found
	  
	uint64_t new_page = l1d_record_pages_table[cpu][index_record].linnea;
	uint64_t new_offset = l1d_record_pages_table[cpu][index_record].last_offset;
	int new_stride;
	if (!l1d_current_pages_table[cpu][index].short_reuse) {
	  new_stride = l1d_current_pages_table[cpu][index].stride;
	} else {
	  assert(!l1d_ip_table[cpu][ip_index].short_reuse);
	  new_stride = l1d_ip_table[cpu][ip_index].berti_or_pointer;
	}
	//if (ip_index == 0x10f) cout << "LONG REUSE PREFETCH " << hex << page_addr << "->" << new_page << " " << ip_index << dec << " " << new_offset << " " << new_stride << " " << where << endl;
	  
	// Need to add the linnea page to current pages
	uint64_t new_index = l1d_get_current_pages_entry(new_page);
	  
	if (new_index == L1D_CURRENT_PAGES_TABLE_ENTRIES) {
	    
	  // Add new page entry evicting a previous one.
	  new_index = l1d_evict_lru_current_page_entry();
	  l1d_add_current_pages_table(new_index, new_page);
	    
	}
	      
	uint64_t pf_offset = new_offset + new_stride;
	if (pf_offset < L1D_PAGE_BLOCKS) {
	  uint64_t new_line = new_page << L1D_PAGE_BLOCKS_BITS;
	  uint64_t new_pf_line = new_line | pf_offset;
	  uint64_t new_addr = new_line << LOG2_BLOCK_SIZE;
	  uint64_t new_pf_addr = new_pf_line << LOG2_BLOCK_SIZE;
	    
	  //cout << "LINNEA " << hex << new_page << " " << dec << pf_offset << hex << " " << " " << new_line << " " << new_pf_line << " " << new_addr << " " << new_pf_addr << dec << endl;
	    
	  //if (ip_index == 0x10f) cout << "STRIDE PREFETCH " << hex << new_page << dec << " <" << pf_offset << ">" << endl;
	  bool prefetched = prefetch_line(new_pf_addr, true, (LOAD_TYPE) ld_type, (count_reuse < 3) ? 0 : 1);
	  assert(prefetched);
	  l1d_add_latencies_table(new_index, pf_offset, current_cycle);
	}
      }
    }
  }
#endif
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  //cout << "EVICT " << hex << v_evicted_addr << dec << endl;
  //if (((v_evicted_addr >> LOG2_BLOCK_SIZE) >> L1D_PAGE_BLOCKS_BITS) == 0x28e837c8b) cout << "@" << current_cycle << " EVICT " << hex << ((v_evicted_addr >> LOG2_BLOCK_SIZE) >> L1D_PAGE_BLOCKS_BITS) << dec << " <" << ((v_evicted_addr >> LOG2_BLOCK_SIZE) & L1D_PAGE_OFFSET_MASK) << ">" << endl;

  //if (((v_addr >> LOG2_BLOCK_SIZE) >> L1D_PAGE_BLOCKS_BITS) == 0x28e837c8b) cout << "@" << current_cycle << " FILL " << hex << ((v_addr >> LOG2_BLOCK_SIZE) >> L1D_PAGE_BLOCKS_BITS) << dec << " <" << ((v_addr >> LOG2_BLOCK_SIZE) & L1D_PAGE_OFFSET_MASK) << ">" << endl;
  
  l1d_cpu_id = cpu;
  uint64_t line_addr = (addr >> LOG2_BLOCK_SIZE);
  uint64_t page_addr = line_addr >> L1D_PAGE_BLOCKS_BITS;
  uint64_t offset = line_addr & L1D_PAGE_OFFSET_MASK;
  
  uint64_t pointer_prev = l1d_get_current_pages_entry(page_addr);

  if (pointer_prev < L1D_CURRENT_PAGES_TABLE_ENTRIES) { // look in prev requests
    
    // First look in prefetcher, since if there is a hit, it is the time the miss started
    uint64_t latency = l1d_get_and_set_latency_latencies_table(pointer_prev, offset, current_cycle);
     
    if (latency != 0) {
      
      // Find berti (distance from pref_latency + demand_latency cycles before
      int berti[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
      unsigned saved_cycles[L1D_CURRENT_PAGES_TABLE_NUM_BERTI_PER_ACCESS];
      l1d_get_berti_prev_requests_table(pointer_prev, offset, latency, berti, saved_cycles, 0);
      /* if (page_addr == 0x28e837ca4) { */
      /* 	cout << "@" << current_cycle << " (" << latency << ") ADD BERTI MISS " << pointer_prev << " <" << offset << "> "; */
      /* 	for (int i = 0; berti[i] != 0; i++) { */
      /* 	  cout << berti[i] << " " << saved_cycles[i] << ", "; */
      /* 	} */
      /* 	cout << endl; */
      /* 	l1d_print_prev_requests_table(pointer_prev); */
      /* } */
      l1d_add_berti_current_pages_table(pointer_prev, berti, saved_cycles);
      
    } // If not found, berti will not be found neither
  } // If not found, not entry in prev requests

  // If the replacement is in the prev req, invalidate the entry (not usefull anymore)
  uint64_t evicted_page = (evicted_addr >> LOG2_BLOCK_SIZE) >> L1D_PAGE_BLOCKS_BITS;
  uint64_t evicted_index = l1d_get_current_pages_entry(evicted_page);
  if (evicted_index < L1D_CURRENT_PAGES_TABLE_ENTRIES) {
    uint64_t evicted_offset = (evicted_addr >> LOG2_BLOCK_SIZE) & L1D_PAGE_OFFSET_MASK;
    l1d_reset_entry_latencies_table(evicted_index, evicted_offset);
    //l1d_evict_current_page_entry(evicted_index);
  }
  return metadata_in;
}

void CACHE::prefetcher_final_stats()
{
  cout << "CPU " << cpu << " L1D berti prefetcher final stats" << endl;
  uint64_t max_misses = 0;
  uint64_t max_pos = 0;
  uint64_t ip_cache_misses = 0;
  // for (uint64_t i = 0; i < L1D_IP_TABLE_ENTRIES; i++) {
  //   //if (max_misses < l1d_ip_misses[cpu][i]) {
  //   //  max_misses = l1d_ip_misses[cpu][i]; // * 100) / l1d_ip_hits[cpu][i];
  //   //  max_pos = i;
  //   //}
  //   ip_cache_misses += l1d_ip_late[cpu][i] + l1d_ip_early[cpu][i] + l1d_ip_misses[cpu][i];
  //   int all_ip = l1d_ip_hits[cpu][i] + l1d_ip_late[cpu][i] + l1d_ip_early[cpu][i] + l1d_ip_misses[cpu][i];
  //   if (all_ip) {
  //     cout << hex << i << dec
	//    << "-> Hits: " << l1d_ip_hits[cpu][i]
	//    << ", Late: "<< l1d_ip_late[cpu][i]
	//    << ", Early: "<< l1d_ip_early[cpu][i]
	//    << ", Misses: "<< l1d_ip_misses[cpu][i] << endl;
  //   }
  // }
  cout << "Total misses (from ip) " << ip_cache_misses << endl;
  cout << "Total accesses (misses) " << cache_accesses << " " << cache_misses << endl;
  cout << "Pref: Addr " << l1d_stats_pref_addr << " IP " << l1d_stats_pref_ip << " Current " << l1d_stats_pref_current << endl;
}

void CACHE::prefetcher_squash(uint64_t ip, uint64_t instr_id) {}
void CACHE::prefetcher_cycle_operate() {}