#include <catch.hpp>

#include <map>
#include <nlohmann/json.hpp>

#include "environment.h"
#include "instr.h"
#include "modules.h"
#include "instruction_producer.h"

namespace champsim
{
void assign_identities(modules::environment_module& env);
}

namespace
{

struct probe_producer_505 : public champsim::modules::instruction_producer {
  explicit probe_producer_505(champsim::modules::ModuleBuilder builder) { producer_group_ = builder.get_parameter<std::string>("producer_group", true, std::string{}); }
  const ooo_model_instr* peek() override { return nullptr; }
  void consume() override {}
  [[nodiscard]] bool eof() const override { return true; }
};

static champsim::modules::instruction_producer::register_module<probe_producer_505> probe_producer_reg("PROBE_PRODUCER_505");

struct probe_core_505 : public champsim::modules::core_module {
  explicit probe_core_505(champsim::modules::ModuleBuilder builder) : core_module(champsim::chrono::picoseconds{250})
  {
    for (const auto& sub : builder.get_submodules("instruction_producer", true)) {
      champsim::modules::instruction_producer::create_instance(sub, static_cast<champsim::modules::packet_consumer*>(this));
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

static champsim::modules::core_module::register_module<probe_core_505> probe_core_reg("PROBE_CORE_505");

} // namespace

TEST_CASE("num_consumers, num_producers, and num_producer_groups are each counted in their own space")
{
  // Two consumers (cores). Four producers: c0 holds three (two sharing a
  // producer_group label, one on its own), c1 holds one. Three producer groups: the
  // shared label collapses c0's pair onto one id. Every count differs (2, 4, 3),
  // so any conflation fails loudly.
  nlohmann::json config = {
      {"children",
       nlohmann::json::array({
           nlohmann::json{
               {"name", "t505_c0"},
               {"module", "core"},
               {"model", "PROBE_CORE_505"},
               {"children",
                nlohmann::json::array({
                    nlohmann::json{{"name", "t505_c0_srcA"}, {"module", "instruction_producer"}, {"model", "PROBE_PRODUCER_505"}, {"producer_group", "t505_shared"}},
                    nlohmann::json{{"name", "t505_c0_srcB"}, {"module", "instruction_producer"}, {"model", "PROBE_PRODUCER_505"}, {"producer_group", "t505_shared"}},
                    nlohmann::json{{"name", "t505_c0_srcC"}, {"module", "instruction_producer"}, {"model", "PROBE_PRODUCER_505"}},
                })},
           },
           nlohmann::json{
               {"name", "t505_c1"},
               {"module", "core"},
               {"model", "PROBE_CORE_505"},
               {"children",
                nlohmann::json::array({
                    nlohmann::json{{"name", "t505_c1_src"}, {"module", "instruction_producer"}, {"model", "PROBE_PRODUCER_505"}},
                })},
           },
       })},
  };

  auto env_builder = champsim::modules::ModuleBuilder("t505_env", "ENVIRONMENT").add_parameter("config_json", config).add_parameter("cli_args",
                                                                                                                                    nlohmann::json::object());
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  REQUIRE(env != nullptr);

  auto& globals = champsim::modules::ModuleBuilder::globals();
  CHECK(globals.get_parameter<std::size_t>("num_consumers") == 2);
  CHECK(globals.get_parameter<std::size_t>("num_producers") == 4);
  CHECK(globals.get_parameter<std::size_t>("num_producer_groups") == 3);

  // c0's labeled pair share one producer id; c0's third producer and c1's producer
  // each get their own. (View order is top-level modules before nested ones, so
  // assertions key on names, not positions.)
  champsim::assign_identities(*env);
  auto producers = env->typed_view<champsim::modules::packet_producer>("packet_producer");
  REQUIRE(std::size(producers) == 4);
  std::map<std::string, uint32_t> producer_of;
  for (auto& src : producers) {
    producer_of[src.get().producer_name()] = src.get().producer_id();
  }
  REQUIRE(std::size(producer_of) == 4);
  CHECK(producer_of.at("t505_c0_srcA") == producer_of.at("t505_c0_srcB")); // shared label, one producer id
  CHECK(producer_of.at("t505_c0_srcC") != producer_of.at("t505_c0_srcA"));
  CHECK(producer_of.at("t505_c1_src") != producer_of.at("t505_c0_srcA"));
  CHECK(producer_of.at("t505_c1_src") != producer_of.at("t505_c0_srcC"));
}
