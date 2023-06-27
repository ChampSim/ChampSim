#include <catch.hpp>

#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

#include "channel.h"

TEST_CASE("The MSHR factor uses the number of upper levels to determine the PTW's default number of MSHRs") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  auto num_uls = GENERATE(1u,2u,4u,6u);
  constexpr int mshr_factor = 2;
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  REQUIRE(uut.MSHR_SIZE == mshr_factor*num_uls);
}

TEST_CASE("The MSHR factor can control the PTW's default number of MSHRs") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  std::size_t num_uls = 2;
  auto mshr_factor = GENERATE(1,2,4,6);
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  REQUIRE(uut.MSHR_SIZE == mshr_factor*num_uls);
}

TEST_CASE("Specifying the PTW's MSHR size overrides the MSHR factor") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  std::size_t num_uls = 2;
  auto mshr_factor = 2;
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  buildA.mshr_size(6);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  REQUIRE(uut.MSHR_SIZE == 6);
}

TEST_CASE("The bandwidth factor uses the number of upper levels to determine the PTW's default tag and fill bandwidth") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  auto num_uls = GENERATE(1u,2u,4u,6u);
  auto bandwidth_factor = 2;
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  CHECK(uut.MAX_READ == bandwidth_factor*num_uls);
  CHECK(uut.MAX_FILL == bandwidth_factor*num_uls);
}

TEST_CASE("The bandwidth factor can control the PTW's default tag bandwidth") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  auto num_uls = 2u;
  auto bandwidth_factor = GENERATE(1,2,4,6);
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  CHECK(uut.MAX_READ == bandwidth_factor*num_uls);
  CHECK(uut.MAX_FILL == bandwidth_factor*num_uls);
}

TEST_CASE("Specifying the tag bandwidth overrides the PTW's bandwidth factor") {
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
  VirtualMemory vmem{1<<12, 4, 200, dram};

  auto num_uls = 2u;
  auto bandwidth_factor = 2;
  PageTableWalker::Builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  buildA.tag_bandwidth(6);
  buildA.fill_bandwidth(7);

  PageTableWalker uut{buildA.virtual_memory(&vmem)};

  CHECK(uut.MAX_READ == 6);
  CHECK(uut.MAX_FILL == 7);
}


