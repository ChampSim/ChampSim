#include <catch.hpp>
#include <nlohmann/json.hpp>

#include "environment.h"
#include "identity_registry.h"
#include "instr.h"
#include "modules.h"
#include "instruction_producer.h"

namespace champsim
{
void assign_identities(modules::environment_module& env);
} // namespace champsim

namespace
{

// A producer that accepts the optional "producer_group" sharing label
struct labeled_producer_087 : public champsim::modules::instruction_producer {
  explicit labeled_producer_087(champsim::modules::ModuleBuilder builder) { producer_group_ = builder.get_parameter<std::string>("producer_group", true, std::string{}); }
  const ooo_model_instr* peek() override { return nullptr; }
  void consume() override {}
  [[nodiscard]] bool eof() const override { return true; }
};

static champsim::modules::instruction_producer::register_module<labeled_producer_087> labeled_producer_reg("LABELED_PRODUCER_087");

struct id_core_087 : public champsim::modules::core_module {
  std::vector<labeled_producer_087*> producers_;

  explicit id_core_087(champsim::modules::ModuleBuilder builder) : core_module(champsim::chrono::picoseconds{250})
  {
    for (const auto& sub : builder.get_submodules("instruction_producer", true)) {
      producers_.push_back(
          dynamic_cast<labeled_producer_087*>(champsim::modules::instruction_producer::create_instance(sub, static_cast<champsim::modules::packet_consumer*>(this))));
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

static champsim::modules::core_module::register_module<id_core_087> id_core_reg("ID_CORE_087");

nlohmann::json producer_json(const std::string& name, const std::string& label = {})
{
  nlohmann::json src{{"name", name}, {"module", "instruction_producer"}, {"model", "LABELED_PRODUCER_087"}};
  if (!label.empty()) {
    src["producer_group"] = label;
  }
  return src;
}

} // namespace

TEST_CASE("Identities are assigned internally: dense consumers, per-producer ids, shared labels")
{
  // Three consumers. c0 holds two producers sharing a label with c2's producer;
  // c1's producer is unlabeled.
  nlohmann::json config = {
      {"children", nlohmann::json::array({
                       nlohmann::json{{"name", "c0"},
                                      {"module", "core"},
                                      {"model", "ID_CORE_087"},
                                      {"children", nlohmann::json::array({producer_json("c0_a", "brown"), producer_json("c0_b")})}},
                       nlohmann::json{{"name", "c1"},
                                      {"module", "core"},
                                      {"model", "ID_CORE_087"},
                                      {"children", nlohmann::json::array({producer_json("c1_a")})}},
                       nlohmann::json{{"name", "c2"},
                                      {"module", "core"},
                                      {"model", "ID_CORE_087"},
                                      {"children", nlohmann::json::array({producer_json("c2_a", "brown")})}},
                   })},
  };

  auto env_builder =
      champsim::modules::ModuleBuilder("t087_env", "ENVIRONMENT").add_parameter("config_json", config).add_parameter("cli_args", nlohmann::json::object());
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  REQUIRE(env != nullptr);

  champsim::assign_identities(*env);

  auto cores = env->typed_view<champsim::modules::core_module>("core");
  REQUIRE(cores.size() == 3);
  auto& c0 = dynamic_cast<id_core_087&>(cores.at(0).get());
  auto& c1 = dynamic_cast<id_core_087&>(cores.at(1).get());
  auto& c2 = dynamic_cast<id_core_087&>(cores.at(2).get());

  SECTION("Consumer ids are dense in configuration order")
  {
    REQUIRE(c0.consumer_id() == 0);
    REQUIRE(c1.consumer_id() == 1);
    REQUIRE(c2.consumer_id() == 2);
  }

  SECTION("Unlabeled producers each own a distinct producer id")
  {
    REQUIRE(c0.producers_.at(1)->producer_id() != c0.producers_.at(0)->producer_id());
    REQUIRE(c1.producers_.at(0)->producer_id() != c0.producers_.at(0)->producer_id());
    REQUIRE(c1.producers_.at(0)->producer_id() != c0.producers_.at(1)->producer_id());
  }

  SECTION("Producers sharing a label share one producer id, across consumers")
  {
    REQUIRE(c0.producers_.at(0)->producer_id() == c2.producers_.at(0)->producer_id());
  }

  SECTION("The identity registry translates ids to configured names and back")
  {
    REQUIRE(champsim::identities().consumer_name(1) == std::optional<std::string>{"c1"});
    REQUIRE(champsim::identities().consumer_id("c2") == std::optional<int>{2});
    REQUIRE(champsim::identities().producer_id("c1_a") == std::optional<uint32_t>{c1.producers_.at(0)->producer_id()});

    auto shared = champsim::identities().packet_producers(c0.producers_.at(0)->producer_id());
    REQUIRE_THAT(shared, Catch::Matchers::UnorderedEquals(std::vector<std::string>{"c0_a", "c2_a"}));

    REQUIRE_FALSE(champsim::identities().consumer_name(99).has_value());
    REQUIRE_FALSE(champsim::identities().consumer_id("nonesuch").has_value());
  }
}
