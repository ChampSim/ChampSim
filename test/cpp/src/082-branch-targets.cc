#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch.hpp>
#include <vector>

#include "tracereader.h"
#include "instr.h"

namespace
{
ooo_model_instr non_branch_inst(champsim::address ip)
{
  return champsim::test::instruction_with_ip(ip);
}

ooo_model_instr not_taken_inst(champsim::address ip)
{
  auto i = non_branch_inst(ip);
  i.is_branch = true;
  i.branch_taken = false;
  return i;
}

ooo_model_instr taken_inst(champsim::address ip)
{
  auto i = non_branch_inst(ip);
  i.is_branch = true;
  i.branch_taken = true;
  return i;
}
}

TEST_CASE("A sequence of instructions has correct branch targets for taken branches")
{
  std::vector<ooo_model_instr> generated_instrs{};
  champsim::address start_address{0x44440000};
  std::generate_n(std::back_inserter(generated_instrs), 10, [start_address]() mutable {
    auto i = ::taken_inst(start_address);
    start_address += 0x100;
    return i;
  });

  champsim::set_branch_targets(std::begin(generated_instrs), std::end(generated_instrs));

  std::vector<std::pair<champsim::address, champsim::address>> ip_target_pairs{};
  std::transform(std::next(std::begin(generated_instrs)), std::end(generated_instrs), std::begin(generated_instrs), std::back_inserter(ip_target_pairs), [](const auto& target, const auto& branch){
    return std::pair{branch.branch_target, target.ip};
  });

  REQUIRE_THAT(ip_target_pairs, Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::pair<champsim::address, champsim::address>>(
    [](const auto& val) { return val.first == val.second; },
    "Matches IP and target"
  )));
}

TEST_CASE("A sequence of instructions has no branch targets for not-taken branches")
{
  std::vector<ooo_model_instr> generated_instrs{};
  champsim::address start_address{0x44440000};
  std::generate_n(std::back_inserter(generated_instrs), 10, [start_address]() mutable {
    auto i = ::not_taken_inst(start_address);
    start_address += 0x100;
    return i;
  });

  champsim::set_branch_targets(std::begin(generated_instrs), std::end(generated_instrs));

  std::vector<std::pair<champsim::address, champsim::address>> ip_target_pairs{};
  std::transform(std::next(std::begin(generated_instrs)), std::end(generated_instrs), std::begin(generated_instrs), std::back_inserter(ip_target_pairs), [](const auto& target, const auto& branch){
    return std::pair{branch.branch_target, target.ip};
  });


  REQUIRE_THAT(ip_target_pairs, Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::pair<champsim::address, champsim::address>>(
    [](const auto& val) { return val.first == champsim::address{}; },
    "Does not have branch target"
  )));
}

TEST_CASE("A sequence of instructions has no branch targets for non-branches")
{
  std::vector<ooo_model_instr> generated_instrs{};
  champsim::address start_address{0x44440000};
  std::generate_n(std::back_inserter(generated_instrs), 10, [start_address]() mutable {
    auto i = ::non_branch_inst(start_address);
    start_address += 0x100;
    return i;
  });

  champsim::set_branch_targets(std::begin(generated_instrs), std::end(generated_instrs));

  std::vector<std::pair<champsim::address, champsim::address>> ip_target_pairs{};
  std::transform(std::next(std::begin(generated_instrs)), std::end(generated_instrs), std::begin(generated_instrs), std::back_inserter(ip_target_pairs), [](const auto& target, const auto& branch){
    return std::pair{branch.branch_target, target.ip};
  });


  REQUIRE_THAT(ip_target_pairs, Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::pair<champsim::address, champsim::address>>(
    [](const auto& val) { return val.first == champsim::address{}; },
    "Does not have branch target"
  )));
}
