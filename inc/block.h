#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"
#include "instruction.h"
#include "set.h"

#include <limits>

// CACHE BLOCK
struct BLOCK {
    uint8_t valid = 0,
            prefetch = 0,
            dirty = 0,
            used = 0;

    int delta = 0,
        depth = 0,
        signature = 0,
        confidence = 0;

    uint64_t address = 0,
             full_addr = 0,
	     v_address = 0,
	     full_v_addr = 0,
             tag = 0,
             data = 0,
             ip,
             cpu = 0,
             instr_id = 0;

    // replacement state
    uint32_t lru = 0;
};

// DRAM CACHE BLOCK
struct DRAM_ARRAY {
    BLOCK **block = NULL;
};

// message packet
struct PACKET {
    uint8_t instruction = 0,
            is_data = 1,
            fill_l1i = 0,
            fill_l1d = 0,
            tlb_access = 0,
            scheduled = 0,
            translated = 0,
            fetched = 0,
            prefetched = 0,
            drc_tag_read = 0;

    int fill_level = -1,
        pf_origin_level,
        rob_signal = -1,
        rob_index = -1,
        producer = -1,
        delta = 0,
        depth = 0,
        signature = 0,
        confidence = 0;

    uint32_t pf_metadata;

    uint8_t  is_producer = 0,
             //rob_index_depend_on_me[ROB_SIZE],
             //lq_index_depend_on_me[ROB_SIZE],
             //sq_index_depend_on_me[ROB_SIZE],
             instr_merged = 0,
             load_merged = 0,
             store_merged = 0,
             returned = 0,
             asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()},
             type = 0;

    fastset
             rob_index_depend_on_me,
             lq_index_depend_on_me,
             sq_index_depend_on_me;

    uint32_t cpu = NUM_CPUS, data_index = 0, lq_index = 0, sq_index = 0;

    uint64_t address = 0,
             full_addr = 0,
             instruction_pa = 0,
             v_address = 0,
             full_v_addr = 0,
             data_pa,
             data = 0,
             instr_id = 0,
             ip = 0,
             event_cycle = std::numeric_limits<uint64_t>::max(),
             cycle_enqueued = 0;
};

// packet queue
struct PACKET_QUEUE {
    string NAME;
    uint32_t SIZE;

    uint8_t  is_RQ = 0,
             is_WQ = 0,
             write_mode;

    uint32_t cpu = 0,
             head = 0,
             tail = 0,
             occupancy = 0,
             num_returned = 0,
             next_fill_index = 0,
             next_schedule_index = 0,
             next_process_index = 0;

    uint64_t next_fill_cycle = std::numeric_limits<uint64_t>::max(),
             next_schedule_cycle = std::numeric_limits<uint64_t>::max(),
             next_process_cycle = std::numeric_limits<uint64_t>::max(),
             ACCESS = 0,
             FORWARD = 0,
             MERGED = 0,
             TO_CACHE = 0,
             ROW_BUFFER_HIT = 0,
             ROW_BUFFER_MISS = 0,
             FULL = 0;

    PACKET *entry, processed_packet[2*MAX_READ_PER_CYCLE];

    // constructor
    PACKET_QUEUE(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        write_mode = 0;
        entry = new PACKET[SIZE];
    };

    PACKET_QUEUE() {}

    // destructor
    ~PACKET_QUEUE() {
        delete[] entry;
    };

    // functions
    int check_queue(PACKET* packet);
    void add_queue(PACKET* packet),
         remove_queue(PACKET* packet);
};

// reorder buffer
struct CORE_BUFFER {
    const string NAME;
    const uint32_t SIZE;
    uint32_t cpu,
             head = 0,
             tail = 0,
             occupancy = 0,
             last_read, last_fetch, last_scheduled = 0,
             inorder_fetch[2] = {},
             next_fetch[2] = {},
             next_schedule = 0;
    uint64_t event_cycle = 0,
             fetch_event_cycle = std::numeric_limits<uint64_t>::max(),
             schedule_event_cycle = std::numeric_limits<uint64_t>::max(),
             execute_event_cycle = std::numeric_limits<uint64_t>::max(),
             lsq_event_cycle = std::numeric_limits<uint64_t>::max(),
             retire_event_cycle = std::numeric_limits<uint64_t>::max();

    ooo_model_instr *entry;

    // constructor
    CORE_BUFFER(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        last_read = SIZE-1;
        last_fetch = SIZE-1;
        entry = new ooo_model_instr[SIZE];
    };

    // destructor
    ~CORE_BUFFER() {
        delete[] entry;
    };
};

// load/store queue
struct LSQ_ENTRY {
    uint64_t instr_id = 0,
             producer_id = std::numeric_limits<uint64_t>::max(),
             virtual_address = 0,
             physical_address = 0,
             ip = 0,
             event_cycle = 0;

    uint32_t rob_index = 0, data_index = 0, sq_index = std::numeric_limits<uint32_t>::max();

    uint8_t translated = 0,
            fetched = 0,
            asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
// forwarding_depend_on_me[ROB_SIZE];
    fastset
		forwarding_depend_on_me;
};

struct LOAD_STORE_QUEUE {
    const string NAME;
    const uint32_t SIZE;
    uint32_t occupancy = 0, head = 0, tail = 0;

    LSQ_ENTRY *entry;

    // constructor
    LOAD_STORE_QUEUE(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        entry = new LSQ_ENTRY[SIZE];
    };

    // destructor
    ~LOAD_STORE_QUEUE() {
        delete[] entry;
    };
};
#endif

