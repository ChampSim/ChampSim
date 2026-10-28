#include <catch.hpp>
#include <nlohmann/json.hpp>

#include "environment.h"
#include "instr.h"
#include "modules.h"
#include "instruction_producer.h"

namespace
{

// Probe producer: records what its constructor resolves for a set of knobs
struct probe_producer_504 : public champsim::modules::instruction_producer {
  long seen_shared, seen_inner, seen_parent_local;

  explicit probe_producer_504(champsim::modules::ModuleBuilder builder)
      : seen_shared(builder.get_parameter<long>("t504_shared_knob", true, -1)), seen_inner(builder.get_parameter<long>("t504_inner_knob", true, -1)),
        seen_parent_local(builder.get_parameter<long>("t504_parent_local", true, -1))
  {
  }
  const ooo_model_instr* peek() override { return nullptr; }
  void consume() override {}
  [[nodiscard]] bool eof() const override { return true; }
};

static champsim::modules::instruction_producer::register_module<probe_producer_504> probe_producer_reg("PROBE_PRODUCER_504");

// Probe core: records its own resolutions and constructs its producer children
struct probe_core_504 : public champsim::modules::core_module {
  long seen_shared, seen_explicit;
  std::vector<probe_producer_504*> producers_;

  explicit probe_core_504(champsim::modules::ModuleBuilder builder)
      : core_module(champsim::chrono::picoseconds{250}), seen_shared(builder.get_parameter<long>("t504_shared_knob", true, -1)),
        seen_explicit(builder.get_parameter<long>("t504_explicit_knob", true, -1))
  {
    for (const auto& sub : builder.get_submodules("instruction_producer", true)) {
      producers_.push_back(dynamic_cast<probe_producer_504*>(
          champsim::modules::instruction_producer::create_instance(sub, static_cast<champsim::modules::packet_consumer*>(this))));
    }
  }

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return 0; }
  uint64_t sim_cycle() const override { return 0; }
  long operate() override { return 0; }
  cpu_stats get_sim_stats() const override { return {}; }
  bool producers_eof() const override { return true; }
};

static champsim::modules::core_module::register_module<probe_core_504> probe_core_reg("PROBE_CORE_504");

} // namespace

TEST_CASE("Top-level config scalars are globals and 'globals' blocks scope lexically")
{
  nlohmann::json config = {
      // bare top-level scalar: a global, visible everywhere
      {"t504_shared_knob", 7},
      // explicit root globals block: equivalent standing
      {"globals", nlohmann::json{{"t504_explicit_knob", 11}}},
      {"children",
       nlohmann::json::array({
           // c0 opens a scope shadowing the root value, and adds a scope-only knob;
           // it also has an ordinary (module-local) parameter
           nlohmann::json{
               {"name", "c0"},
               {"module", "core"},
               {"model", "PROBE_CORE_504"},
               {"globals", nlohmann::json{{"t504_shared_knob", 9}, {"t504_inner_knob", 3}}},
               {"t504_parent_local", 5},
               {"children",
                nlohmann::json::array({
                    nlohmann::json{{"name", "c0_src"}, {"module", "instruction_producer"}, {"model", "PROBE_PRODUCER_504"}},
                    nlohmann::json{{"name", "c0_src_shadow"},
                                   {"module", "instruction_producer"},
                                   {"model", "PROBE_PRODUCER_504"},
                                   {"t504_shared_knob", 1}},
                })},
           },
           // c1 has no scope of its own: it sees the root values
           nlohmann::json{{"name", "c1"}, {"module", "core"}, {"model", "PROBE_CORE_504"}},
       })},
  };

  auto env_builder = champsim::modules::ModuleBuilder("t504_env", "ENVIRONMENT").add_parameter("config_json", config).add_parameter("cli_args",
                                                                                                                                    nlohmann::json::object());
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  REQUIRE(env != nullptr);

  auto cores = env->typed_view<champsim::modules::core_module>("core");
  REQUIRE(cores.size() == 2);
  auto& c0 = dynamic_cast<probe_core_504&>(cores.at(0).get());
  auto& c1 = dynamic_cast<probe_core_504&>(cores.at(1).get());
  REQUIRE(c0.producers_.size() == 2);

  SECTION("A bare top-level scalar is visible as a global")
  {
    REQUIRE(c1.seen_shared == 7);
  }

  SECTION("A root 'globals' block is equivalent to bare scalars")
  {
    REQUIRE(c0.seen_explicit == 11);
    REQUIRE(c1.seen_explicit == 11);
  }

  SECTION("A module's 'globals' block shadows the root for itself and its subtree")
  {
    REQUIRE(c0.seen_shared == 9);
    REQUIRE(c0.producers_.at(0)->seen_shared == 9);
    REQUIRE(c0.producers_.at(0)->seen_inner == 3);
  }

  SECTION("A local parameter shadows every enclosing scope")
  {
    REQUIRE(c0.producers_.at(1)->seen_shared == 1);
  }

  SECTION("Ordinary module parameters stay module-local")
  {
    REQUIRE(c0.producers_.at(0)->seen_parent_local == -1);
  }

  SECTION("Scopes do not leak to siblings")
  {
    REQUIRE(c1.seen_shared == 7);
  }
}
