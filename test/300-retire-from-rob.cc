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

SCENARIO("Completed instructions are retired") {
  GIVEN("An empty ROB") {
    do_nothing_MRC mock_ITLB, mock_DTLB, mock_L1I, mock_L1D;
    O3_CPU uut(0, 1.0, 32, 8, 2, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D, O3_CPU::bpred_t::bbranchDbimodal, O3_CPU::btb_t::bbtbDbasic_btb);

    WHEN("A cycle happens") {
      auto old_num_retired = uut.num_retired;
      for (auto op : std::array<champsim::operable*,5>{&uut, &mock_ITLB, &mock_DTLB, &mock_L1I, &mock_L1D})
        op->_operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }
  }
}
