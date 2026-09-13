#include <catch2/catch_test_macros.hpp>
#include <any>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "chrono.h"
#include "core_stats.h"
#include "modules.h"
#include "operable.h"
#include "phase_controller.h"
#include "phase_info.h"

// champsim::main drives the real phase loop *and* the per-controller stat collection. That collection
// (collect_phase_stats, scoped to each controller's governed set) is file-local to champsim.cc, so the
// main loop is the only way to exercise it — a phase_controller unit test cannot reach it.
namespace champsim
{
std::vector<phase_stats> main(modules::environment_module& env);
}

namespace
{
using champsim::modules::ModuleBuilder;

// A core that retires one instruction per cycle and tags its stat line/JSON with its own stamped name,
// so a collected phase's stats can be traced back to the exact module that produced them.
struct stat_core_916 : public champsim::modules::core_module {
  uint64_t instr_count = 0;
  explicit stat_core_916(ModuleBuilder) : core_module(champsim::chrono::picoseconds{250}) {}

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return instr_count; }
  uint64_t sim_cycle() const override { return 0; }
  long operate() override
  {
    ++instr_count;
    return 1;
  }
  cpu_stats get_sim_stats() const override { return {}; }
  bool producers_eof() const override { return false; }

  void end_phase(champsim::stat_report& out) override
  {
    out.line("MOCKSTAT " + stat_name());
    out.json().add("who", stat_name());
  }
};
static champsim::modules::core_module::register_module<stat_core_916> stat_core_reg_916("STAT_CORE_916");

// Environment serving exactly the views champsim::main needs, holding the two controllers it drives.
struct stat_env_916 : public champsim::modules::environment_module {
  std::vector<stat_core_916*> cores_;
  std::vector<champsim::modules::phase_controller*> controllers_;
  explicit stat_env_916(ModuleBuilder) {}

  std::vector<std::any> view(const std::string& t) const override
  {
    std::vector<std::any> r;
    if (t == "operable")
      for (auto* c : cores_)
        r.push_back(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c)));
    else if (t == "module_lifecycle")
      for (auto* c : cores_)
        r.push_back(static_cast<champsim::module_lifecycle*>(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c))));
    else if (t == "packet_consumer")
      for (auto* c : cores_)
        r.push_back(static_cast<champsim::modules::packet_consumer*>(static_cast<champsim::modules::core_module*>(c)));
    else if (t == "core")
      for (auto* c : cores_)
        r.push_back(static_cast<champsim::modules::core_module*>(c));
    else if (t == "phase_controller")
      for (auto* pc : controllers_)
        r.push_back(pc);
    return r; // "packet_producer" / "vmem" intentionally resolve to an empty view
  }

  int get_deadlock_cycles() const override { return 100000; }
  const ModuleBuilder get_builder_params(const std::string&) const override { return ModuleBuilder(); }
};
static champsim::modules::environment_module::register_module<stat_env_916> stat_env_reg_916("STAT_ENV_916");

champsim::module_lifecycle* as_lifecycle(stat_core_916* c)
{
  return static_cast<champsim::module_lifecycle*>(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c)));
}

bool contains(const std::vector<std::string>& lines, const std::string& needle)
{
  for (const auto& l : lines)
    if (l.find(needle) != std::string::npos)
      return true;
  return false;
}

} // namespace

TEST_CASE("Two controllers with disjoint governance collect independent, correctly-scoped stats")
{
  auto env_builder = ModuleBuilder("env_916", "STAT_ENV_916");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* menv = dynamic_cast<stat_env_916*>(env);

  auto* coreA = dynamic_cast<stat_core_916*>(champsim::modules::core_module::create_instance(ModuleBuilder("coreA", "STAT_CORE_916"), env));
  auto* coreB = dynamic_cast<stat_core_916*>(champsim::modules::core_module::create_instance(ModuleBuilder("coreB", "STAT_CORE_916"), env));
  menv->cores_ = {coreA, coreB};

  // Controller A governs coreA with a single ROI phase.
  nlohmann::json a_phases = nlohmann::json::array();
  a_phases.push_back({{"name", "MeasuredA"}, {"is_warmup", false}, {"length", 5}});
  auto* pcA = champsim::modules::phase_controller::create_instance(ModuleBuilder("pcA", "PHASE_CONTROLLER")
                                                                       .add_parameter("deadlock_cycles", 100000)
                                                                       .add_parameter("governs", std::vector<champsim::module_lifecycle*>{as_lifecycle(coreA)})
                                                                       .add_parameter("phases", a_phases),
                                                                   env);

  // Controller B governs coreB with a *different* plan: a warmup phase and a non-warmup but
  // explicitly unmeasured phase (neither of which may be collected), then a differently-named ROI
  // phase of a different length.
  nlohmann::json b_phases = nlohmann::json::array();
  b_phases.push_back({{"name", "WarmB"}, {"is_warmup", true}, {"length", 2}});
  b_phases.push_back({{"name", "FastB"}, {"is_warmup", false}, {"roi", false}, {"length", 3}});
  b_phases.push_back({{"name", "MeasuredB"}, {"is_warmup", false}, {"length", 6}});
  auto* pcB = champsim::modules::phase_controller::create_instance(ModuleBuilder("pcB", "PHASE_CONTROLLER")
                                                                       .add_parameter("deadlock_cycles", 100000)
                                                                       .add_parameter("governs", std::vector<champsim::module_lifecycle*>{as_lifecycle(coreB)})
                                                                       .add_parameter("phases", b_phases),
                                                                   env);
  menv->controllers_ = {pcA, pcB};

  // main() -> assign_identities bounds-checks the discovered consumers against the global num_consumers
  // (default 1 per test); this run has two, so raise it to match.
  ModuleBuilder::globals().add_parameter("num_consumers", std::size_t{2});

  auto results = champsim::main(*env);

  // Exactly the two ROI phases were collected; B's warmup and unmeasured phases produced no stats block.
  REQUIRE(results.size() == 2);
  auto by_name = [&](const std::string& n) -> const champsim::phase_stats* {
    for (const auto& s : results)
      if (s.name == n)
        return &s;
    return nullptr;
  };
  REQUIRE(by_name("WarmB") == nullptr);
  REQUIRE(by_name("FastB") == nullptr); // non-warmup, but roi:false -- the controller discards its report
  const auto* A = by_name("MeasuredA");
  const auto* B = by_name("MeasuredB");
  REQUIRE(A != nullptr);
  REQUIRE(B != nullptr);

  // Plaintext is scoped to each controller's governed set: A sees only coreA, B only coreB — proving the
  // two controllers collect independently and neither clobbers the other's stats.
  REQUIRE(contains(A->lines, "MOCKSTAT coreA"));
  REQUIRE_FALSE(contains(A->lines, "MOCKSTAT coreB"));
  REQUIRE(contains(B->lines, "MOCKSTAT coreB"));
  REQUIRE_FALSE(contains(B->lines, "MOCKSTAT coreA"));

  // JSON is scoped the same way: one "core" entry per phase, keyed by the governed module's name.
  REQUIRE(A->stats.at("core").at("STAT_CORE_916").size() == 1);
  REQUIRE(A->stats.at("core").at("STAT_CORE_916").contains("coreA"));
  REQUIRE(B->stats.at("core").at("STAT_CORE_916").size() == 1);
  REQUIRE(B->stats.at("core").at("STAT_CORE_916").contains("coreB"));
}
