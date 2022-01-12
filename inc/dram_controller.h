#ifndef DRAM_H
#define DRAM_H

#include <array>
#include <cmath>
#include <limits>

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"

// these values control when to send out a burst of writes
#define DRAM_WRITE_HIGH_WM    ((DRAM_WQ_SIZE*7)>>3) // 7/8th
#define DRAM_WRITE_LOW_WM     ((DRAM_WQ_SIZE*3)>>2) // 6/8th
#define MIN_DRAM_WRITES_PER_SWITCH (DRAM_WQ_SIZE*1/4)

struct BANK_REQUEST {
    bool valid = false,
         row_buffer_hit = false;

    std::size_t open_row = std::numeric_limits<uint32_t>::max();

    uint64_t event_cycle = 0;

    std::vector<PACKET>::iterator pkt;
};

struct DRAM_CHANNEL
{
    std::vector<PACKET> WQ{DRAM_WQ_SIZE};
    std::vector<PACKET> RQ{DRAM_RQ_SIZE};

    std::array<BANK_REQUEST, DRAM_RANKS*DRAM_BANKS> bank_request = {};
    std::array<BANK_REQUEST, DRAM_RANKS*DRAM_BANKS>::iterator active_request = std::end(bank_request);

    uint64_t dbus_cycle_available = 0,
             dbus_cycle_congested = 0,
             dbus_count_congested = 0;

    bool write_mode = false;

    unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer {
  public:
    // DRAM_IO_FREQ defined in champsim_constants.h
    const static uint64_t tRP                        = std::ceil(1.0 * tRP_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
    const static uint64_t tRCD                       = std::ceil(1.0 * tRCD_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
    const static uint64_t tCAS                       = std::ceil(1.0 * tCAS_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
    const static uint64_t DRAM_DBUS_TURN_AROUND_TIME = std::ceil(1.0 * DBUS_TURN_AROUND_NANOSECONDS * DRAM_IO_FREQ / 1000);
    const static uint64_t DRAM_DBUS_RETURN_TIME      = std::ceil(1.0 * BLOCK_SIZE / DRAM_CHANNEL_WIDTH);

    std::array<DRAM_CHANNEL, DRAM_CHANNELS> channels;

    MEMORY_CONTROLLER(double freq_scale) : champsim::operable(freq_scale), MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {}

    int  add_rq(PACKET packet) override,
         add_wq(PACKET packet) override,
         add_pq(PACKET packet) override;

    void operate();

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    void schedule(std::vector<PACKET>::iterator q_it);

    uint32_t dram_get_channel(uint64_t address),
             dram_get_rank   (uint64_t address),
             dram_get_bank   (uint64_t address),
             dram_get_row    (uint64_t address),
             dram_get_column (uint64_t address);
};

#endif

