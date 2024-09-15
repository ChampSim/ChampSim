#include <catch.hpp>

#include "dram_controller.h"

TEST_CASE("The channel has the specified number of rows") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8},8, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};
  REQUIRE(uut.address_mapping.rows() == rows);
}

TEST_CASE("The channel has the specified number of columns") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};
  REQUIRE(uut.address_mapping.columns() == columns);
}

TEST_CASE("The channel has the specified number of ranks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};
  REQUIRE(uut.address_mapping.ranks() == ranks);
}

TEST_CASE("The channel has the specified number of banks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{8}, 1, 1, mapper};
  REQUIRE(uut.address_mapping.banks() == banks);
}

TEST_CASE("The bank request capacity is the product of the bank count and the rank count") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, mapper};
  REQUIRE(uut.bank_request_capacity() == uut.address_mapping.ranks()*uut.address_mapping.banks());
}
