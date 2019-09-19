#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>

#include <iostream>
#include <queue>
#include <map>
#include <random>
#include <string>
#include <iomanip>

// USEFUL MACROS
//#define DEBUG_PRINT
#define SANITY_CHECK
#define LLC_BYPASS
#define DRC_BYPASS
#define NO_CRC2_COMPILE

#ifdef DEBUG_PRINT
#define DP(x) x
#else
#define DP(x)
#endif

// CPU
#define NUM_CPUS 1
#define CPU_FREQ 4000
#define DRAM_IO_FREQ 3200
#define PAGE_SIZE 4096
#define LOG2_PAGE_SIZE 12

// CACHE
#define BLOCK_SIZE 64
#define LOG2_BLOCK_SIZE 6
#define MAX_READ_PER_CYCLE 8
#define MAX_FILL_PER_CYCLE 1

#define INFLIGHT 1
#define COMPLETED 2

#define FILL_L1    1
#define FILL_L2    2
#define FILL_LLC   4
#define FILL_DRC   8
#define FILL_DRAM 16

// DRAM
#define DRAM_CHANNELS 1      // default: assuming one DIMM per one channel 4GB * 1 => 4GB off-chip memory
#define LOG2_DRAM_CHANNELS 0
#define DRAM_RANKS 1         // 512MB * 8 ranks => 4GB per DIMM
#define LOG2_DRAM_RANKS 0
#define DRAM_BANKS 8         // 64MB * 8 banks => 512MB per rank
#define LOG2_DRAM_BANKS 3
#define DRAM_ROWS 65536      // 2KB * 32K rows => 64MB per bank
#define LOG2_DRAM_ROWS 16
#define DRAM_COLUMNS 128      // 64B * 32 column chunks (Assuming 1B DRAM cell * 8 chips * 8 transactions = 64B size of column chunks) => 2KB per row
#define LOG2_DRAM_COLUMNS 7
#define DRAM_ROW_SIZE (BLOCK_SIZE*DRAM_COLUMNS/1024)

#define DRAM_SIZE (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024) 
#define DRAM_PAGES ((DRAM_SIZE<<10)>>2) 
//#define DRAM_PAGES 10

using namespace std;

extern uint8_t warmup_complete[NUM_CPUS], 
               simulation_complete[NUM_CPUS], 
               all_warmup_complete, 
               all_simulation_complete,
               MAX_INSTR_DESTINATIONS,
               knob_cloudsuite,
               knob_low_bandwidth;

extern uint64_t current_core_cycle[NUM_CPUS], 
                stall_cycle[NUM_CPUS], 
                last_drc_read_mode, 
                last_drc_write_mode,
                drc_blocks;

extern queue <uint64_t> page_queue;
extern map <uint64_t, uint64_t> page_table, inverse_table, recent_page, unique_cl[NUM_CPUS];
extern uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS], allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS], major_fault[NUM_CPUS];

void print_stats();
uint64_t rotl64 (uint64_t n, unsigned int c),
         rotr64 (uint64_t n, unsigned int c),
         va_to_pa(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage);

// log base 2 function from efectiu
int lg2(int n);

// smart random number generator
class RANDOM {
  public:
    std::random_device rd;
    std::mt19937_64 engine{rd()};
    std::uniform_int_distribution<uint64_t> dist{0, 0xFFFFFFFFF}; // used to generate random physical page numbers

    RANDOM (uint64_t seed) {
        engine.seed(seed);
    }

    uint64_t draw_rand() {
        return dist(engine);
    };
};
extern uint64_t champsim_seed;
#endif
