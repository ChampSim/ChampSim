#include <catch.hpp>

#include "dram_controller.h"

TEST_CASE("The channel has the specified number of rows") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 2,4);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto slicer = DRAM_CHANNEL::make_slicer(8, rows, columns, ranks, banks);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, slicer};
  REQUIRE(uut.rows() == rows);
}

TEST_CASE("The channel has the specified number of columns") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 2,4);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto slicer = DRAM_CHANNEL::make_slicer(8, rows, columns, ranks, banks);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, slicer};
  REQUIRE(uut.columns() == columns);
}

TEST_CASE("The channel has the specified number of ranks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 2,4);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto slicer = DRAM_CHANNEL::make_slicer(8, rows, columns, ranks, banks);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, slicer};
  REQUIRE(uut.ranks() == ranks);
}

TEST_CASE("The channel has the specified number of banks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 2,4);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto slicer = DRAM_CHANNEL::make_slicer(8, rows, columns, ranks, banks);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, slicer};
  REQUIRE(uut.banks() == banks);
}

TEST_CASE("The bank request capacity is the product of the bank count and the rank count") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 2,4);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto slicer = DRAM_CHANNEL::make_slicer(8, rows, columns, ranks, banks);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{1}, champsim::data::bytes{1}, 1, 1, slicer};
  REQUIRE(uut.bank_request_capacity() == uut.ranks()*uut.banks());
}
