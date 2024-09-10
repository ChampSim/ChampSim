#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    

bool generate_packet(champsim::channel& channel,uint64_t packet_num, champsim::data::bytes dram_size, champsim::data::bytes block_size, std::vector<uint64_t>& expected_returns)
{
    auto pkt_type = (packet_num % 2 == 0) ? access_type::LOAD : access_type::WRITE;
    champsim::channel::request_type r;
    r.type = pkt_type;
    r.address = champsim::address(packet_num * block_size.count());
    r.v_address = champsim::address{};
    r.instr_id = 0;
    r.response_requested = false;

    //if load, response_request = true
    if(r.type == access_type::LOAD)
    {
        r.response_requested = true;
        expected_returns.push_back(r.address.to<uint64_t>());
        return(channel.add_rq(r));
    }
    else
        return(channel.add_wq(r));
}
void return_test(MEMORY_CONTROLLER& uut, champsim::channel& channel_uut, uint64_t packets, champsim::data::bytes dram_size, champsim::data::bytes block_size, std::vector<uint64_t>& expected_returns)
{
    //send out all packets
    for(uint64_t packets_sent  = 0; packets_sent < packets; packets_sent++)
    {
        //generate a packet and send it
        bool success = false;
        while(!success)
        {
            success = generate_packet(channel_uut, packets_sent, dram_size, block_size, expected_returns);
            //operate mem controller
            uut._operate();
        }
    }

    //wait for return queue to get all packets
    for(int i = 0; i < 700; i++){ uut._operate(); };
}

SCENARIO("A dram controller returns reads") {
    GIVEN("A series of reads into the dram controller") {
        const auto clock_period = champsim::chrono::picoseconds{3200};
        champsim::channel channel_uut{32, 32, 32, champsim::data::bits{8}, false};
        const uint64_t trp_cycles = 4;
        const uint64_t trcd_cycles = 4;
        const uint64_t tcas_cycles = 80;
        const std::size_t DRAM_CHANNELS = 2;
        const std::size_t DRAM_BANKS = 8;
        const std::size_t DRAM_RANKS = 8;
        const std::size_t DRAM_COLUMNS = 128;
        const std::size_t DRAM_ROWS = 65536;
        const std::size_t DRAM_ROWS_P_REF = 8;

        const uint64_t packets_issued = 64;

        std::vector<uint64_t> expected_returns;
        std::vector<uint64_t> actual_returns;

        MEMORY_CONTROLLER uut{clock_period, trp_cycles*clock_period, trcd_cycles*clock_period, tcas_cycles*clock_period, champsim::chrono::microseconds(64000), 2*clock_period, {&channel_uut}, 64, 64, DRAM_CHANNELS, champsim::data::bytes{8}, DRAM_ROWS, DRAM_COLUMNS, DRAM_RANKS, DRAM_BANKS, DRAM_ROWS_P_REF};
        WHEN("The reads are issued") {
            return_test(uut,channel_uut,packets_issued,uut.size(),champsim::data::bytes(BLOCK_SIZE), expected_returns);
            std::transform(channel_uut.returned.begin(), channel_uut.returned.end(), std::back_inserter(actual_returns), [](champsim::channel::response_type r){return(r.address.to<uint64_t>());});
            THEN("The packets are returned on the upstream channel")
            {
                REQUIRE_THAT(actual_returns, Catch::Matchers::UnorderedEquals(expected_returns));
            }
        }
    }
}

