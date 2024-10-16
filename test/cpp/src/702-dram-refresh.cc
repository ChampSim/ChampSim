#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <map>

bool generate_packet(champsim::channel* channel,uint64_t packet_num, uint64_t DRAM_CHANNELS, uint64_t DRAM_RANKS, uint64_t DRAM_BANKS, uint64_t DRAM_COLUMNS, std::map<champsim::address,uint64_t>& arrival_times, uint64_t current_cycle)
{
    //this generates packets that all go to the same area. We expect latencies to be around tCAS.
    auto pkt_type = access_type::LOAD;
    champsim::channel::request_type r;
    r.type = pkt_type;
    uint64_t chan_size = 8;
    uint64_t offset = 0;
    std::size_t PREFETCH_SIZE = 8;

    champsim::address_slice block_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(chan_size*PREFETCH_SIZE)}, champsim::data::bits{offset}}, 0};
    offset += champsim::lg2(chan_size*PREFETCH_SIZE);
    champsim::address_slice channel_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_CHANNELS) + offset}, champsim::data::bits{offset}}, 0};
    offset += champsim::lg2(DRAM_CHANNELS);
    champsim::address_slice bank_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_BANKS) + offset}, champsim::data::bits{offset}}, 0};
    offset += champsim::lg2(DRAM_BANKS);
    champsim::address_slice column_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_COLUMNS/PREFETCH_SIZE) + offset}, champsim::data::bits{offset}}, packet_num % DRAM_COLUMNS};
    offset += champsim::lg2(DRAM_COLUMNS/PREFETCH_SIZE);
    champsim::address_slice rank_slice{champsim::dynamic_extent{champsim::data::bits{champsim::lg2(DRAM_RANKS) + offset}, champsim::data::bits{offset}}, 0};
    offset += champsim::lg2(DRAM_RANKS);
    champsim::address_slice row_slice{champsim::dynamic_extent{champsim::data::bits{64}, champsim::data::bits{offset}}, 0};
    r.address = champsim::address{champsim::splice(row_slice, rank_slice, column_slice, bank_slice, channel_slice, block_slice)};
    r.v_address = champsim::address{};
    r.instr_id = 0;
    r.data = champsim::address{packet_num};
    r.response_requested = true;


    bool success = channel->add_rq(r);

    if(success)
        arrival_times[r.data] = current_cycle;

    return(success);
}

std::vector<bool> refresh_test(MEMORY_CONTROLLER* uut, champsim::channel* channel_uut, uint64_t refresh_cycles, uint64_t DRAM_CHANNELS, uint64_t DRAM_RANKS, uint64_t DRAM_BANKS, uint64_t DRAM_COLUMNS, champsim::chrono::picoseconds tREF, std::vector<uint64_t>& latencies)
{
    //times packets were sent out
    std::map<champsim::address,uint64_t> arrival_times;

    //record the refresh status of each bank
    std::vector<bool> bank_refreshed(DRAM_BANKS,false);

    //num of refresh cycles to cover
    //record whether each refresh cycle was respected
    std::vector<bool> refresh_done;

    //we will cover the first 2 refreshes
    uint64_t refresh_cycle = 2;
    uint64_t cycles = 0;
    uint64_t packets_sent = 0;
    while (uut->current_time < champsim::chrono::clock::time_point{} + (tREF*refresh_cycles))
    {
        //check for returns and record latencies
        for(auto& ret : channel_uut->returned)
        {
            //fmt::print("Packet arrived at {} and left at {}\n", arrival_times[ret.data], cycles);
            latencies.push_back(cycles - arrival_times[ret.data]);
        }

        channel_uut->returned.clear();
        //generate a random packet every 500 cycles to the same address (latency should be low, unless refresh is underway)
        if(cycles % 500 == 0)
        {
            bool packet_sent = generate_packet(channel_uut,packets_sent, DRAM_CHANNELS, DRAM_RANKS, DRAM_BANKS, DRAM_COLUMNS, arrival_times, cycles);
            //operate mem controller
            if(packet_sent)
                packets_sent++;
        }

        //operate mc
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
        const std::size_t DRAM_BANKS = 4;
        const std::size_t DRAM_BANKGROUPS = 8;
        const std::size_t DRAM_RANKS = 1;
        const std::size_t DRAM_COLUMNS = 1024;
        const std::size_t DRAM_ROWS = 65536;

        //test a total of 4 different configurations of refresh periods and refreshes per period
        auto refresh_period = GENERATE(as<champsim::chrono::microseconds>{}, 64000, 32000);
        auto REFRESHES_PER_PERIOD = GENERATE(as<std::size_t>{}, 8192, 16384);
        const champsim::chrono::picoseconds tREF{refresh_period / REFRESHES_PER_PERIOD};
        

        MEMORY_CONTROLLER uut{champsim::chrono::picoseconds{312}, champsim::chrono::picoseconds{624}, std::size_t{24}, std::size_t{24}, std::size_t{24},std::size_t{52}, refresh_period, {&channel_uut}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{8}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKGROUPS, DRAM_BANKS, REFRESHES_PER_PERIOD};
        uut.warmup = false;
        uut.channels[0].warmup = false;

        std::vector<uint64_t> packet_latencies;

        WHEN("The memory controller is operated over 40 refresh cycles alongside many accesses to the same column") {
            std::vector<bool> refresh_status = refresh_test(&uut,&channel_uut,40,DRAM_CHANNELS, DRAM_RANKS, DRAM_BANKS, DRAM_COLUMNS, tREF, packet_latencies);
            THEN("Each bank undergoes refresh according to specified timing")
            {
                REQUIRE_THAT(refresh_status, Catch::Matchers::AllTrue());
            }
            THEN("Latency difference between accesses is the refresh time")
            {
                uint64_t max_latency = *std::max_element(std::begin(packet_latencies),std::end(packet_latencies));
                uint64_t min_latency = *std::min_element(std::begin(packet_latencies),std::end(packet_latencies));
                champsim::data::gibibytes density = champsim::data::bytes(DRAM_BANKS * DRAM_BANKGROUPS * DRAM_COLUMNS * DRAM_ROWS);
                uint64_t expected_refresh_latency = (uint64_t)std::sqrt((double)density.count() * 8.00)*(std::size_t{52});
                uint64_t apparent_refresh_latency = max_latency - min_latency;
                uint64_t variance = 15; //necessary, because of packet mergers and fr-fcfs scheduling

                REQUIRE(apparent_refresh_latency < expected_refresh_latency + variance);
                REQUIRE(apparent_refresh_latency > expected_refresh_latency - variance);
            }
        }
    }
}
