#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    
#include "champsim_constants.h"
#include <algorithm>
#include <cfenv>
#include <cmath>

void generate_packet(champsim::channel* channel)
{
    auto pkt_type = rand() % 2 ? access_type::LOAD : access_type::WRITE;
    champsim::channel::request_type r;
    r.type = pkt_type;
    uint64_t offset = 0;
    r.address = 0;
    r.v_address = 0;
    offset += LOG2_BLOCK_SIZE;
    r.address += 0 << offset;
    offset += champsim::lg2(DRAM_CHANNELS);
    r.address += rand() % DRAM_BANKS << offset;
    offset += champsim::lg2(DRAM_BANKS);
    r.address += 1 << offset;
    offset += champsim::lg2(DRAM_COLUMNS);
    r.address += rand() % DRAM_RANKS << offset;
    offset += champsim::lg2(DRAM_RANKS);
    r.address += rand() % DRAM_ROWS << offset;
    r.instr_id = 0;
    r.response_requested = false;

    if(r.type == access_type::LOAD)
        channel->add_rq(r);
    else
        channel->add_wq(r);
}

std::vector<bool> refresh_test(MEMORY_CONTROLLER* uut, champsim::channel* channel_uut, uint64_t refresh_cycles)
{
    uut->current_cycle = 1;
    //how many cycles should pass before the next refresh is scheduled. This is also the maximum time that can pass before
    //a refresh MUST be done. If this is violated, then DRAM spec is violated.
    uint64_t refresh_rate = uint64_t((DRAM_IO_FREQ * 1e6 * 0.064) / (DRAM_ROWS/(double)8));

    //record the refresh status of each bank
    std::vector<bool> bank_refreshed(DRAM_BANKS,false);

    //num of refresh cycles to cover
    //record whether each refresh cycle was respected
    std::vector<bool> refresh_done;

    //we will cover the first 80 refreshes
    while (uut->current_cycle < refresh_rate*refresh_cycles)
    {
        //generate a random packet
        generate_packet(channel_uut);
        //operate mem controller
        uut->_operate();
        
        //make sure that for every refresh cycle, each bank undergoes refresh at least once
        std::vector<bool> bank_under_refresh;
        std::transform(std::begin(uut->channels[0].bank_request),std::end(uut->channels[0].bank_request),std::back_inserter(bank_under_refresh),[](const auto& entry){return(entry.under_refresh);});
        std::transform(std::begin(bank_refreshed),std::end(bank_refreshed),std::begin(bank_under_refresh),std::begin(bank_refreshed), std::logical_or<>{});
        
        if(uut->current_cycle % refresh_rate == 0)
        {
            refresh_done.push_back(std::all_of(std::begin(bank_refreshed),std::end(bank_refreshed),[](bool v) { return v;}));
            std::fill(std::begin(bank_refreshed),std::end(bank_refreshed),false);
        }
    }
    return(refresh_done);
}

SCENARIO("The memory controller refreshes each bank at the proper rate") {
    GIVEN("A random request stream to the memory controller") {
        champsim::channel channel_uut{32, 32, 32, LOG2_BLOCK_SIZE, false};
        MEMORY_CONTROLLER uut{1, 3200, 12.5, 12.5, 20, 7.5, {&channel_uut}};
        //test
        uut.warmup = false;
        uut.channels[0].warmup = false;
        //packets need address
        /*
        * | row address | rank index | column address | bank index | channel | block
        * offset |
        */
        WHEN("The memory controller is operated over 40 refresh cycles") {
            std::vector<bool> refresh_status = refresh_test(&uut,&channel_uut,40);
            THEN("Each bank undergoes refresh according to specified timing")
            {
                REQUIRE_THAT(refresh_status, Catch::Matchers::AllTrue());
            }
        }
    }
}

