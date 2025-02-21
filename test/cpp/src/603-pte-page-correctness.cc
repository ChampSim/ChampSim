#include <array>

#include "catch.hpp"
#include "defaults.hpp"
#include "dram_controller.h"
#include "mocks.hpp"
#include "ptw.h"
#include "vmem.h"

SCENARIO("The page table steps have correct offsets") {
  auto level = GENERATE(as<unsigned>{}, 1,2,3,4,5);
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{640}, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("603-uut-"+std::to_string(level))
      .clock_period(champsim::chrono::picoseconds{3200})
      //.rq_size(16)
      //.tag_bandwidth(2)
      //.fill_bandwidth(2)
      //.mshr_size(5)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .add_pscl(5,1,1)
      .add_pscl(4,1,1)
      .add_pscl(3,1,1)
      .add_pscl(2,1,1)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    //uint64_t addr = (0xffff'ffff'ffe0'0000 | ((3*(level+1)) << LOG2_PAGE_SIZE)) << (level * 9);

    //level 1 -> 12 bits
    //level 2 -> 21 bits
    //level 3 -> 30 bits
    //level 4 -> 39 bits
    //level 5 -> 48 bits
                                                    //  5    4    3    2   1
    champsim::address addr0{0x0005'0200'c040'1000}; // 0x5, 0x4, 0x3, 0x2, 0x1
    champsim::address addr1{0x0006'0281'0060'2000}; // 0x6, 0x5, 0x3, 0x3, 0x2
    champsim::address addr2{0x0007'0301'4080'3000}; // 0x7, 0x6, 0x3, 0x4, 0x3
    champsim::address addr3{0x0008'0381'80a0'4000}; // 0x8, 0x7, 0x3, 0x5, 0x4
    champsim::address addr4{0x0009'0401'c0c0'5000}; // 0x9, 0x8, 0x3, 0x6, 0x5
    champsim::address addr5{0x000a'0482'00e0'6000}; // 0xa, 0x9, 0x3, 0x7, 0x6
    champsim::address addr6{0x000b'0502'4100'7000}; // 0xb, 0xa, 0x3, 0x8, 0x7
    champsim::address addr7{0x000c'0582'8120'8000}; // 0xc, 0xb, 0x3, 0x9, 0x8
    std::vector<champsim::address> addresses = {addr0, addr1, addr2, addr3, addr4, addr5, addr6, addr7};

    for(auto addr : addresses) {
      WHEN("The PTW receives a request") {
        decltype(mock_ul)::request_type test;
        test.address = addr;
        test.v_address = test.address;
        test.cpu = 0;

        auto test_result = mock_ul.issue(test);
        REQUIRE(test_result);

        for (auto i = 0; i < 10000; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The " + std::to_string(level) + "th request has the correct offset") {
          using namespace champsim::data::data_literals;
          REQUIRE(mock_ll.packet_count() == levels);
          fmt::print("level: {} got: {} expected: {}\n",level,mock_ll.addresses.at(levels-level).slice_lower(12_b).to<std::size_t>(),level * pte_entry::byte_multiple);
          REQUIRE(mock_ll.addresses.at(levels-level).slice_lower(12_b).to<std::size_t>() == level * pte_entry::byte_multiple);
        }
      }
    }
  }
}
