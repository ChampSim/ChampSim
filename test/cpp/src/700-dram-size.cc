#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    

SCENARIO("A dram controller reports its size accurately") {
    GIVEN("A DRAM configuration of 8GB") {
        const auto clock_period = champsim::chrono::picoseconds{3200};
        const uint64_t trp_cycles = 4;
        const uint64_t trcd_cycles = 4;
        const uint64_t tcas_cycles = 80;
        const std::size_t DRAM_CHANNELS = 2;
        const std::size_t DRAM_BANKS = 8;
        const std::size_t DRAM_RANKS = 8;
        const std::size_t DRAM_COLUMNS = 128;
        const std::size_t DRAM_ROWS = 65536;
        const std::size_t DRAM_CHANNEL_WIDTH = 8;
        const champsim::data::bytes expected_size{1ul << 33ul};

        MEMORY_CONTROLLER uut{clock_period, trp_cycles*clock_period, trcd_cycles*clock_period, tcas_cycles*clock_period, 2*clock_period, {}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{DRAM_CHANNEL_WIDTH}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKS};
        WHEN("The memory controller is queried for size") {
            champsim::data::bytes actual_size = uut.size();
            THEN("The memory controller reports the correct size")
            {
                REQUIRE(actual_size.count() == expected_size.count());
            }
        }
    }
    GIVEN("A DRAM configuration of 16GB") {
        const auto clock_period = champsim::chrono::picoseconds{3200};
        const uint64_t trp_cycles = 4;
        const uint64_t trcd_cycles = 4;
        const uint64_t tcas_cycles = 80;
        const std::size_t DRAM_CHANNELS = 4;
        const std::size_t DRAM_BANKS = 8;
        const std::size_t DRAM_RANKS = 8;
        const std::size_t DRAM_COLUMNS = 128;
        const std::size_t DRAM_ROWS = 65536;
        const std::size_t DRAM_CHANNEL_WIDTH = 8;
        const champsim::data::gibibytes expected_size{1ul << 34ul};

        MEMORY_CONTROLLER uut{clock_period, trp_cycles*clock_period, trcd_cycles*clock_period, tcas_cycles*clock_period, 2*clock_period, {}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{DRAM_CHANNEL_WIDTH}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKS};
        WHEN("The memory controller is queried for size") {
            champsim::data::bytes actual_size = uut.size();
            THEN("The memory controller reports the correct size")
            {
                REQUIRE(actual_size.count() == expected_size.count());
            }
        }
    }
    GIVEN("A DRAM configuration of 32GB") {
        const auto clock_period = champsim::chrono::picoseconds{3200};
        const uint64_t trp_cycles = 4;
        const uint64_t trcd_cycles = 4;
        const uint64_t tcas_cycles = 80;
        const std::size_t DRAM_CHANNELS = 4;
        const std::size_t DRAM_BANKS = 16;
        const std::size_t DRAM_RANKS = 8;
        const std::size_t DRAM_COLUMNS = 128;
        const std::size_t DRAM_ROWS = 65536;
        const std::size_t DRAM_CHANNEL_WIDTH = 8;
        const champsim::data::gibibytes expected_size{1ul << 35ul};

        MEMORY_CONTROLLER uut{clock_period, trp_cycles*clock_period, trcd_cycles*clock_period, tcas_cycles*clock_period, 2*clock_period, {}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{DRAM_CHANNEL_WIDTH}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKS};
        WHEN("The memory controller is asked for the size of memory") {
            champsim::data::bytes actual_size = uut.size();
            THEN("The memory controller reports the correct size")
            {
                REQUIRE(actual_size.count() == expected_size.count());
            }
        }
    }
}

