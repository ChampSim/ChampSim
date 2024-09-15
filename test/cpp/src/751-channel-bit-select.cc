#include <catch.hpp>
#include <fmt/core.h>

#include "dram_controller.h"

TEST_CASE("A DRAM channel can identify the bits of a row") {
  auto rows = GENERATE(as<std::size_t>{}, 2,8);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,8);
  auto banks = GENERATE(as<std::size_t>{}, 2,8);
  auto prefetch_size = GENERATE(as<std::size_t>{}, 8, 16);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, prefetch_size, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};

  auto segment_to_find = (1ull << champsim::lg2(rows))-1;
  champsim::address addr{segment_to_find << (3 + champsim::lg2(columns) + champsim::lg2(ranks) + champsim::lg2(banks))};

  INFO(fmt::format("address: {}", addr));
  REQUIRE(uut.address_mapping.get_row(addr) == segment_to_find);
}

TEST_CASE("A DRAM channel can identify the bits of a column") {
  auto rows = GENERATE(as<std::size_t>{}, 2,8);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,8);
  auto banks = GENERATE(as<std::size_t>{}, 2,8);
  auto prefetch_size = GENERATE(as<std::size_t>{}, 8, 16);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, prefetch_size, 1, banks, columns, ranks, rows);

  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};

  auto segment_to_find = (1ull << champsim::lg2(columns/prefetch_size))-1;
  champsim::address addr{segment_to_find << (3 + champsim::lg2(banks) + champsim::lg2(prefetch_size))};

  INFO(fmt::format("address: {}", addr));
  REQUIRE(uut.address_mapping.get_column(addr) == segment_to_find);
}

TEST_CASE("A DRAM channel can identify the bits of a rank") {
  auto rows = GENERATE(as<std::size_t>{}, 2,8);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,8);
  auto banks = GENERATE(as<std::size_t>{}, 2,8);
  auto prefetch_size = GENERATE(as<std::size_t>{}, 8, 16);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, prefetch_size, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};

  auto segment_to_find = (1ull << champsim::lg2(ranks))-1;
  champsim::address addr{segment_to_find << (3 + champsim::lg2(columns) + champsim::lg2(banks))};

  INFO(fmt::format("address: {}", addr));
  REQUIRE(uut.address_mapping.get_rank(addr) == segment_to_find);
}

TEST_CASE("A DRAM channel can identify the bits of a bank") {
  auto rows = GENERATE(as<std::size_t>{}, 2,8);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,8);
  auto banks = GENERATE(as<std::size_t>{}, 2,8);
  auto prefetch_size = GENERATE(as<std::size_t>{}, 8, 16);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, prefetch_size, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};

  auto segment_to_find = (1ull << champsim::lg2(banks))-1;
  champsim::address addr{segment_to_find << (3 + champsim::lg2(prefetch_size))};

  INFO(fmt::format("address: {}", addr));
  REQUIRE(uut.address_mapping.get_bank(addr) == segment_to_find);
}
