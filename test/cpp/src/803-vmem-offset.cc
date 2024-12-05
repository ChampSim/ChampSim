#include <catch.hpp>
#include "vmem.h"

#include "dram_controller.h"

TEST_CASE("The virtual memory evaluates the correct shift amounts") {
  constexpr unsigned log2_pte_page_size = 12;

  auto level = GENERATE(as<std::size_t>{}, 1,2,3,4,5);

  MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
  VirtualMemory uut{champsim::data::bytes{1 << log2_pte_page_size}, 5, champsim::chrono::nanoseconds{6400}, dram};

  champsim::data::bits expected_value{LOG2_PAGE_SIZE + (log2_pte_page_size-champsim::lg2(pte_entry::byte_multiple))*(level-1)};
  REQUIRE(uut.shamt(level) == expected_value);
}

TEST_CASE("The virtual memory evaluates the correct offsets") {
  constexpr std::size_t log2_pte_page_size = 12;

  auto level = GENERATE(as<unsigned>{}, 1,2,3,4,5);

  MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
  VirtualMemory uut{champsim::data::bytes{1 << log2_pte_page_size}, 5, champsim::chrono::nanoseconds{6400}, dram};

  champsim::address addr{(0xffff'ffff'ffe0'0000 | (level << LOG2_PAGE_SIZE)) << ((level-1) * 9)};
  REQUIRE(uut.get_offset(addr, level) == level);
}
