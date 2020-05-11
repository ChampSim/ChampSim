#ifndef DRAM_H
#define DRAM_H

#include "memory_class.h"

// DRAM configuration
#define DRAM_CHANNEL_WIDTH 8 // 8B
#define DRAM_WQ_SIZE 64
#define DRAM_RQ_SIZE 64

#define tRP_DRAM_NANOSECONDS  12.5
#define tRCD_DRAM_NANOSECONDS 12.5
#define tCAS_DRAM_NANOSECONDS 12.5

// the data bus must wait this amount of time when switching between reads and writes, and vice versa
#define DRAM_DBUS_TURN_AROUND_TIME ((15*CPU_FREQ)/2000) // 7.5 ns 
extern uint32_t DRAM_MTPS, DRAM_DBUS_RETURN_TIME;

// these values control when to send out a burst of writes
#define DRAM_WRITE_HIGH_WM    ((DRAM_WQ_SIZE*7)>>3) // 7/8th
#define DRAM_WRITE_LOW_WM     ((DRAM_WQ_SIZE*3)>>2) // 6/8th
#define MIN_DRAM_WRITES_PER_SWITCH (DRAM_WQ_SIZE*1/4)

// DRAM
class MEMORY_CONTROLLER : public MEMORY {
  public:
    const string NAME;

    std::array<std::array<std::array<DRAM_ARRAY, DRAM_CHANNELS>, DRAM_RANKS>, DRAM_BANKS> dram_array;
    std::array<uint64_t, DRAM_CHANNELS> dbus_cycle_available;
    std::array<uint64_t, DRAM_CHANNELS> dbus_cycle_congested;
    std::array<std::array<uint64_t, NUM_TYPES+1>, NUM_TYPES+1> dbus_congested;
    std::array<std::array<std::array<uint64_t, DRAM_CHANNELS>, DRAM_RANKS>, DRAM_BANKS> bank_cycle_available;
    uint8_t  do_write;
    std::array<uint8_t, DRAM_CHANNELS> write_mode;
    uint32_t processed_writes;
    std::array<uint32_t, DRAM_CHANNELS> scheduled_reads;
    std::array<uint32_t, DRAM_CHANNELS> scheduled_writes;
    int fill_level;

    std::array<std::array<std::array<BANK_REQUEST, DRAM_CHANNELS>, DRAM_RANKS>, DRAM_BANKS> bank_request;

    // queues
    std::vector<PACKET_QUEUE> WQ;
    std::vector<PACKET_QUEUE> RQ;

    // constructor
    MEMORY_CONTROLLER(string v1) : NAME (v1) {
        for (uint32_t i=0; i<NUM_TYPES+1; i++) {
            dbus_congested[i].fill(0);
        }
        do_write = 0;
        processed_writes = 0;

        dbus_cycle_available.fill(0);
        dbus_cycle_congested.fill(0);
        write_mode.fill(0);
        scheduled_reads.fill(0);
        scheduled_writes.fill(0);

        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            for (uint32_t j=0; j<DRAM_RANKS; j++) {
                bank_cycle_available[i][j].fill(0);
            }

            WQ.emplace_back("DRAM_WQ" + to_string(i), DRAM_WQ_SIZE);
            RQ.emplace_back("DRAM_RQ" + to_string(i), DRAM_RQ_SIZE);
            WQ[i].entry.insert(WQ[i].entry.end(), WQ[i].SIZE, PACKET());
            RQ[i].entry.insert(RQ[i].entry.end(), RQ[i].SIZE, PACKET());
        }

        fill_level = FILL_DRAM;
    };

    // destructor
    ~MEMORY_CONTROLLER() {

    };

    // functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);

    void return_data(PACKET *packet),
         operate(),
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
