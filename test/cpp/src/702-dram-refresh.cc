#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    
#include <algorithm>
#include <cfenv>
#include <cmath>

void generate_packet(champsim::channel* channel,uint64_t packet_num, uint64_t DRAM_CHANNELS, uint64_t DRAM_RANKS, uint64_t DRAM_BANKS, uint64_t DRAM_COLUMNS, uint64_t DRAM_ROWS)
{
    auto pkt_type = packet_num % 2 ? access_type::LOAD : access_type::WRITE;
    champsim::channel::request_type r;
    r.type = pkt_type;
    uint64_t offset = 0;
    champsim::address_slice block_slice{champsim::dynamic_extent{champsim::data::bits{LOG2_BLOCK_SIZE + offset}, champsim::data::bits{offset}}, 0};
    offset += LOG2_BLOCK_SIZE;
    champsim::address_slice channel_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_CHANNELS) + offset}, champsim::data::bits{offset}}, 0};
    offset += champsim::lg2(DRAM_CHANNELS);
    champsim::address_slice bank_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_BANKS) + offset}, champsim::data::bits{offset}}, packet_num % DRAM_BANKS};
    offset += champsim::lg2(DRAM_BANKS);
    champsim::address_slice column_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_COLUMNS) + offset}, champsim::data::bits{offset}}, 1};
    offset += champsim::lg2(DRAM_COLUMNS);
    champsim::address_slice rank_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_RANKS) + offset}, champsim::data::bits{offset}}, packet_num % DRAM_RANKS};
    offset += champsim::lg2(DRAM_RANKS);
    champsim::address_slice row_slice{champsim::dynamic_extent{champsim::data::bits{64}, champsim::data::bits{offset}}, packet_num % DRAM_ROWS};
    r.address = champsim::address{champsim::splice(row_slice, rank_slice, column_slice, bank_slice, channel_slice, block_slice)};
    r.v_address = champsim::address{};
    r.instr_id = 0;
    r.response_requested = false;

    if(r.type == access_type::LOAD)
        channel->add_rq(r);
    else
        channel->add_wq(r);
}

std::vector<bool> refresh_test(MEMORY_CONTROLLER* uut, champsim::channel* channel_uut, uint64_t refresh_cycles, uint64_t DRAM_CHANNELS, uint64_t DRAM_RANKS, uint64_t DRAM_BANKS, uint64_t DRAM_COLUMNS, uint64_t DRAM_ROWS, champsim::chrono::picoseconds tREF)
{
    //a refresh MUST be done. If this is violated, then DRAM spec is violated.
    
    //record the refresh status of each bank
    std::vector<bool> bank_refreshed(DRAM_BANKS,false);

    //advanced current time to first refresh cycle, or test will fail since that is the trigger
    uut->current_time += tREF;
    //num of refresh cycles to cover
    //record whether each refresh cycle was respected
    std::vector<bool> refresh_done;

    //we will cover the first 40 refreshes
    uint64_t refresh_cycle = 2;
    uint64_t cycles = 0;
    while (uut->current_time < champsim::chrono::clock::time_point{} + tREF*refresh_cycles)
    {
        //generate a random packet
        generate_packet(channel_uut,(uint64_t)cycles, DRAM_CHANNELS, DRAM_RANKS, DRAM_BANKS, DRAM_COLUMNS, DRAM_ROWS);
        //operate mem controller
        uut->_operate();
        cycles++;
        //make sure that for every refresh cycle, each bank undergoes refresh at least once
        std::vector<bool> bank_under_refresh;
        std::transform(std::begin(uut->channels[0].bank_request),std::end(uut->channels[0].bank_request),std::back_inserter(bank_under_refresh),[](const auto& entry){return(entry.under_refresh);});
        std::transform(std::begin(bank_refreshed),std::end(bank_refreshed),std::begin(bank_under_refresh),std::begin(bank_refreshed), std::logical_or<>{});
        if(uut->current_time >= champsim::chrono::clock::time_point{} + refresh_cycle*tREF)
        {
            refresh_done.push_back(std::all_of(std::begin(bank_refreshed),std::end(bank_refreshed),[](bool v) { return v;}));
            std::fill(std::begin(bank_refreshed),std::end(bank_refreshed),false);
            refresh_cycle++;
        }
    }
    return(refresh_done);
}

SCENARIO("The memory controller refreshes each bank at the proper rate") {
    GIVEN("A random request stream to the memory controller") {
        champsim::channel channel_uut{32, 32, 32, champsim::data::bits{8}, false};
        const std::size_t DRAM_CHANNELS = 1;
        const std::size_t DRAM_BANKS = 8;
        const std::size_t DRAM_RANKS = 2;
        const std::size_t DRAM_COLUMNS = 128;
        const std::size_t DRAM_ROWS = 65536;
        const champsim::chrono::microseconds refresh_period{64000};
        const champsim::chrono::picoseconds tREF{7812500};
        const std::size_t DRAM_ROW_P_REF = 8;

        MEMORY_CONTROLLER uut{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, refresh_period, champsim::chrono::picoseconds{7500}, {&channel_uut}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{8}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKS,DRAM_ROW_P_REF};
        uut.warmup = false;
        uut.channels[0].warmup = false;
        
        WHEN("The memory controller is operated over 40 refresh cycles") {
            std::vector<bool> refresh_status = refresh_test(&uut,&channel_uut,40,DRAM_CHANNELS, DRAM_RANKS, DRAM_BANKS, DRAM_COLUMNS, DRAM_ROWS, tREF);
            THEN("Each bank undergoes refresh according to specified timing")
            {
                REQUIRE_THAT(refresh_status, Catch::Matchers::AllTrue());
            }
        }
    }
}

