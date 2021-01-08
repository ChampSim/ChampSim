#ifndef DRAM_H
#define DRAM_H

#include "champsim_constants.h"
#include "memory_class.h"
#include "champsim.h"

// the data bus must wait this amount of time when switching between reads and writes, and vice versa
#define DRAM_DBUS_TURN_AROUND_TIME ((15*CPU_FREQ)/2000) // 7.5 ns 
extern uint32_t DRAM_MTPS, DRAM_DBUS_RETURN_TIME;

// these values control when to send out a burst of writes
#define DRAM_WRITE_HIGH_WM    ((DRAM_WQ_SIZE*7)>>3) // 7/8th
#define DRAM_WRITE_LOW_WM     ((DRAM_WQ_SIZE*3)>>2) // 6/8th
#define MIN_DRAM_WRITES_PER_SWITCH (DRAM_WQ_SIZE*1/4)

// DRAM
class MEMORY_CONTROLLER : public MemoryRequestConsumer {
  public:
    const string NAME;

    DRAM_ARRAY dram_array[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];
    uint64_t dbus_cycle_available[DRAM_CHANNELS] = {}, dbus_cycle_congested[DRAM_CHANNELS] = {}, dbus_congested[NUM_TYPES+1][NUM_TYPES+1] = {};
    uint64_t bank_cycle_available[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS] = {};
    uint8_t  do_write = 0, write_mode[DRAM_CHANNELS] = {};
    uint32_t processed_writes = 0, scheduled_reads[DRAM_CHANNELS] = {}, scheduled_writes[DRAM_CHANNELS] = {};
    int fill_level = FILL_DRAM;

    BANK_REQUEST bank_request[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];

    // queues
    PACKET_QUEUE WQ[DRAM_CHANNELS], RQ[DRAM_CHANNELS];

    // constructor
    MEMORY_CONTROLLER(string v1) : NAME (v1) {
        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            WQ[i].NAME = "DRAM_WQ" + to_string(i);
            WQ[i].SIZE = DRAM_WQ_SIZE;
            WQ[i].entry = new PACKET [DRAM_WQ_SIZE];

            RQ[i].NAME = "DRAM_RQ" + to_string(i);
            RQ[i].SIZE = DRAM_RQ_SIZE;
            RQ[i].entry = new PACKET [DRAM_RQ_SIZE];
        }

        fill_level = FILL_DRAM;
    };

    // functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);

    void operate(),
         increment_WQ_FULL(uint64_t address);

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    void schedule(PACKET_QUEUE *queue), process(PACKET_QUEUE *queue),
         update_schedule_cycle(PACKET_QUEUE *queue),
         update_process_cycle(PACKET_QUEUE *queue),
         reset_remain_requests(PACKET_QUEUE *queue, uint32_t channel);

    uint32_t dram_get_channel(uint64_t address),
             dram_get_rank   (uint64_t address),
             dram_get_bank   (uint64_t address),
             dram_get_row    (uint64_t address),
             dram_get_column (uint64_t address),
             drc_check_hit (uint64_t address, uint32_t cpu, uint32_t channel, uint32_t rank, uint32_t bank, uint32_t row);

    uint64_t get_bank_earliest_cycle();

    int check_dram_queue(PACKET_QUEUE *queue, PACKET *packet);
};

#endif

