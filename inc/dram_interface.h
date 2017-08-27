#ifndef DRAM_H
#define DRAM_H

#include "memory_class.h"
#include "DRAMSim.h"

#define DRAM_WQ_SIZE 48
#define DRAM_RQ_SIZE 48

class DRAM_CONTROLLER : public MEMORY {
  public:
    const string NAME;
    DRAMSim::MultiChannelMemorySystem *mem;

    // queues
    PACKET_QUEUE WQ, RQ;

    DRAM_CONTROLLER(string v1) : NAME (v1) {
        mem = DRAMSim::getMemorySystemInstance("ini/DDR2_micron_16M_8b_x8_sg3E.ini", "system.ini", "./DRAMSim2", "example_app", 16384);

        DRAMSim::TransactionCompleteCB *read_cb = new DRAMSim::Callback<DRAM_CONTROLLER, void, unsigned, uint64_t, uint64_t>(this, &DRAM_CONTROLLER::read_complete);
        DRAMSim::TransactionCompleteCB *write_cb = new DRAMSim::Callback<DRAM_CONTROLLER, void, unsigned, uint64_t, uint64_t>(this, &DRAM_CONTROLLER::write_complete);
        mem->RegisterCallbacks(read_cb, write_cb, 0);

        WQ.NAME = "DRAM_WQ";
        WQ.SIZE = DRAM_WQ_SIZE;
        WQ.entry = new PACKET [DRAM_WQ_SIZE];

        RQ.NAME = "DRAM_RQ";
        RQ.SIZE = DRAM_RQ_SIZE;
        RQ.entry = new PACKET [DRAM_RQ_SIZE];
    };

    // destructor
    ~DRAM_CONTROLLER() {

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
    float get_latency();

    int check_dram_queue(PACKET_QUEUE *queue, PACKET *packet);

    // DRAMSIM callbacks
    void read_complete(unsigned, uint64_t, uint64_t);
    void write_complete(unsigned, uint64_t, uint64_t);
};

#endif
