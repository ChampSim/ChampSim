#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "dram_controller.h"    
#include "champsim_constants.h"
#include <algorithm>
#include <cfenv>
#include <cmath>

bool dram_test(MEMORY_CONTROLLER* uut, std::vector<champsim::channel::request_type>* packet_stream, std::vector<uint64_t>* expected_order, std::vector<uint64_t>* arriv_time)
{
    uint64_t vector_index = 0;
    uut->current_cycle = 0;

    std::vector<DRAM_CHANNEL::queue_type::iterator> request_locations;
    //load requests into controller
    for(auto it = packet_stream->begin(); it != packet_stream->end(); it++)
    {
        auto jt = uut->channels[0].RQ.begin() + vector_index;
        *jt = DRAM_CHANNEL::request_type{*it};
        jt->value().forward_checked = false;
        jt->value().event_cycle = (*arriv_time)[vector_index];
        request_locations.push_back(jt);
        vector_index++;
    }

    //carry out operates, make sure they complete in the expected order
    uint64_t completed_reqs = 0;
    std::vector<bool> last_scheduled(packet_stream->size(),false);
    while(true)
    {
        //decide if we are to enqueue a new packet or not
        uut->operate();
        uut->current_cycle++;
        
        //look through RQ, make sure the right packet was scheduled
        uint64_t index = 0;
        for(auto it = uut->channels[0].RQ.begin(); it != uut->channels[0].RQ.end(); it++)
        {
            if(it->has_value())
            if(!last_scheduled[index] && it->has_value() && it->value().scheduled)
            {
                
                if(it != request_locations[(*expected_order)[completed_reqs]])
                {
                
                return(false);
                }
                completed_reqs++;
            }
            if(it->has_value())
            {
                last_scheduled[index] = it->value().scheduled;
            }

            index++;
        }

        //if we are done, break loop
        if(completed_reqs == packet_stream->size())
        break;
    }
    return(true);
}

SCENARIO("A series of reads arrive at the memory controller and are reordered") {
    GIVEN("A request stream to the memory controller") {
        MEMORY_CONTROLLER uut{1, 3200, 12.5, 12.5, 20, 7.5, {}};
        uut.warmup = false;
        //packets need address
        /*
        * | row address | rank index | column address | bank index | channel | block
        * offset |
        */
        std::vector<uint64_t> row_access = {0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0};
        std::vector<uint64_t> col_access = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21};
        std::vector<uint64_t> bak_access = {0,0,0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6};
        std::vector<uint64_t> arriv_time = {3,4,2,0,1,5,6,7,8,9,10,11,12,13,14,15,16,17,20,18,19};
        //we can expect the previous listed accesses to be reordered as such, as long as bank accesses are sufficiently lengthy
        //such that we can allocate requests to 6 additional banks before the first bank is done. The timing for the memory controller
        //is set within this test, so we can always expect this to be the case.
        std::vector<uint64_t> expected_order = {3,2,6,9,12,15,19,4,0,7,10,13,16,20,1,5,8,11,14,17,18};

        std::vector<champsim::channel::request_type> packet_stream;
        for(uint64_t i = 0; i < row_access.size(); i++)
        {
            auto pkt_type = access_type::LOAD;
            champsim::channel::request_type r;
            r.type = pkt_type;
            uint64_t offset = 0;
            r.address = 0;
            r.v_address = 0;
            offset += LOG2_BLOCK_SIZE;
            r.address += 0 << offset;
            offset += champsim::lg2(DRAM_CHANNELS);
            r.address += bak_access[i] << offset;
            offset += champsim::lg2(DRAM_BANKS);
            r.address += col_access[i] << offset;
            offset += champsim::lg2(DRAM_COLUMNS);
            r.address += 0 << offset;
            offset += champsim::lg2(DRAM_RANKS);
            r.address += row_access[i] << offset;
            r.instr_id = i;
            r.response_requested = false;
            packet_stream.push_back(r);
        }
        WHEN("The memory controller is operated") {
            bool result = dram_test(&uut,&packet_stream,&expected_order,&arriv_time);
            THEN("The memory controller scheduled packets according to the FR-FCFS scheme")
            {
                REQUIRE(result);
            }
        }
    }
}

