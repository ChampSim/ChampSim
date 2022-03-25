#include "catch.hpp"

#include <deque>
#include <limits>

#include "memory_class.h"
#include "ooo_cpu.h"
#include "operable.h"

/*
 * A MemoryRequestConsumer that simply returns all packets on the next cycle
 */
class do_nothing_MRC : public MemoryRequestConsumer, public champsim::operable
{
  std::deque<PACKET> packets;
  public:
    do_nothing_MRC() : MemoryRequestConsumer(1), champsim::operable(1) {}

    void operate() {
      for (const PACKET &pkt : packets)
        for (auto ret : pkt.to_return)
          ret->return_data(pkt);
      packets.clear();
    }

    bool add_rq(const PACKET &pkt) { packets.push_back(pkt); return true; }
    bool add_wq(const PACKET &pkt) { packets.push_back(pkt); return true; }
    bool add_pq(const PACKET &pkt) { packets.push_back(pkt); return true; }

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) { return std::size(packets); }
    uint32_t get_size(uint8_t queue_type, uint64_t address) { return std::numeric_limits<uint32_t>::max(); }
};

SCENARIO("The scheduler can detect RAW hazards") {
  GIVEN("A ROB with a single instruction") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_ITLB, mock_DTLB, mock_L1I, mock_L1D;
    O3_CPU uut(0, 1.0, 32, 8, 2, 64, 32, 32, 352, 128, 72, 2, 2, 2, schedule_width, 1, 2, 2, 1, 1, 1, 1, schedule_latency, 0, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D, O3_CPU::bpred_t::bbranchDbimodal, O3_CPU::btb_t::bbtbDbasic_btb);

    uut.ROB.push_back(ooo_model_instr{0, input_instr{}});
    for (auto &instr : uut.ROB)
      instr.event_cycle = uut.current_cycle;

    //auto old_cycle = uut.current_cycle;

    WHEN("The instruction is not scheduled") {
      uut.ROB.front().scheduled = 0;
      for (auto op : std::array<champsim::operable*,5>{&uut, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D})
        op->_operate();

      THEN("The instruction has no register dependencies") {
        REQUIRE(uut.ROB.front().num_reg_dependent == 0);
        REQUIRE(uut.ROB.front().scheduled == COMPLETED);
        //REQUIRE(uut.ROB.front().event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with a RAW hazard") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_ITLB, mock_DTLB, mock_L1I, mock_L1D;
    O3_CPU uut(0, 1.0, 32, 8, 2, 64, 32, 32, 352, 128, 72, 2, 2, 2, schedule_width, 1, 2, 2, 1, 1, 1, 1, schedule_latency, 0, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D, O3_CPU::bpred_t::bbranchDbimodal, O3_CPU::btb_t::bbtbDbasic_btb);

    input_instr dependent_instr;
    dependent_instr.source_registers[0] = 42;
    dependent_instr.destination_registers[0] = 42;
    std::vector test_instructions( 2, ooo_model_instr{0,dependent_instr} );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    for (auto &instr : uut.ROB)
      instr.event_cycle = uut.current_cycle;

    //auto old_cycle = uut.current_cycle;

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB)
        instr.scheduled = 0;

      for (auto op : std::array<champsim::operable*,5>{&uut, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.ROB[0].num_reg_dependent == 0);
        REQUIRE(uut.ROB[1].num_reg_dependent == 1);
        REQUIRE(uut.ROB[0].scheduled == COMPLETED);
        REQUIRE(uut.ROB[1].scheduled == COMPLETED);
        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(uut.ROB[1].event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with more hazards than the scheduler can handle") {
    constexpr unsigned schedule_width = 4;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_ITLB, mock_DTLB, mock_L1I, mock_L1D;
    O3_CPU uut(0, 1.0, 32, 8, 2, 64, 32, 32, 352, 128, 72, 2, 2, 2, schedule_width, 1, 2, 2, 1, 1, 1, 1, schedule_latency, 0, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D, O3_CPU::bpred_t::bbranchDbimodal, O3_CPU::btb_t::bbtbDbasic_btb);

    input_instr dependent_instr;
    dependent_instr.source_registers[0] = 42;
    dependent_instr.destination_registers[0] = 42;
    std::vector test_instructions( schedule_width + 1, ooo_model_instr{0,dependent_instr} );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    int id = 0;
    for (auto &instr : uut.ROB) {
      instr.instr_id = id++;
      instr.event_cycle = uut.current_cycle;
    }

    //auto old_cycle = uut.current_cycle;

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB) {
        instr.scheduled = 0;
        instr.executed = 0;
      }

      for (auto op : std::array<champsim::operable*,5>{&uut, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.ROB.at(0).num_reg_dependent >= 0);
        REQUIRE(uut.ROB.at(1).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(2).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(3).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(4).num_reg_dependent >= 0);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.num_reg_dependent >= 1; }));
        //REQUIRE(uut.ROB.back().num_reg_dependent == 0);

        REQUIRE(uut.ROB.at(0).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(1).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(2).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(3).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(4).scheduled == 0);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.scheduled == COMPLETED; }));
        //REQUIRE(uut.ROB.back().scheduled == 0);

        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [old_cycle](ooo_model_instr x){ return x.event_cycle == old_cycle + schedule_latency; }));
        //REQUIRE(uut.ROB.back().event_cycle == old_cycle);
      }
    }
  }
}

