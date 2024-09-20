#include <catch.hpp>

#include "dram_controller.h"

TEST_CASE("The channel has the specified number of rows") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto uut = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);

  REQUIRE(uut.rows() == rows);
}

TEST_CASE("The channel has the specified number of columns") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto uut = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);

  REQUIRE(uut.columns() == columns);
}

TEST_CASE("The channel has the specified number of ranks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto uut = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);

  REQUIRE(uut.ranks() == ranks);
}

TEST_CASE("The channel has the specified number of banks") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto uut = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);
  REQUIRE(uut.banks() == banks);
}

TEST_CASE("The bank request capacity is the product of the bank count, bankgroup count, and rank count") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{2}, std::size_t{1}, std::size_t{1}, std::size_t{1}, std::size_t{1}, champsim::chrono::microseconds{1}, 1, champsim::data::bytes{1}, 1, 1, mapper};
  REQUIRE(uut.bank_request_capacity() == mapper.ranks()*mapper.banks()*mapper.bankgroups());
}

TEST_CASE("The bankgroup dbus capacity is the product of the bankgroup count and rank count") {
  auto rows = GENERATE(as<std::size_t>{}, 2,4);
  auto columns = GENERATE(as<std::size_t>{}, 128,256);
  auto ranks = GENERATE(as<std::size_t>{}, 2,4);
  auto bankgroups = GENERATE(as<std::size_t>{}, 2,4);
  auto banks = GENERATE(as<std::size_t>{}, 2,4);
  auto mapper = DRAM_ADDRESS_MAPPING(champsim::data::bytes{8}, 8, 1, bankgroups, banks, columns, ranks, rows);
  DRAM_CHANNEL uut{champsim::chrono::picoseconds{1}, champsim::chrono::picoseconds{2}, std::size_t{1}, std::size_t{1}, std::size_t{1}, std::size_t{1}, champsim::chrono::microseconds{1}, 1, champsim::data::bytes{1}, 1, 1, mapper};
  REQUIRE(uut.bankgroup_request_capacity() == mapper.ranks()*mapper.bankgroups());
}
