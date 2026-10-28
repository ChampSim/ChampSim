#include <catch2/catch_test_macros.hpp>
#include <deque>
#include <vector>

#include "instr.h"
#include "modules.h"
#include "instruction_producer.h"

namespace {

// A controllable instruction producer for testing
struct mock_producer_913 : public champsim::modules::instruction_producer {
  std::deque<ooo_model_instr> instructions_;
  bool retire_called_ = false;
  bool squash_called_ = false;
  bool branch_mispredict_called_ = false;

  explicit mock_producer_913(champsim::modules::ModuleBuilder) {}

  const ooo_model_instr* peek() override {
    return instructions_.empty() ? nullptr : &instructions_.front();
  }

  void consume() override { instructions_.pop_front(); }

  [[nodiscard]] bool eof() const override { return instructions_.empty(); }

  void retire_instruction(const ooo_model_instr&) override { retire_called_ = true; }
  void squash_instruction(const ooo_model_instr&) override { squash_called_ = true; }
  void branch_mispredict(const ooo_model_instr&) override { branch_mispredict_called_ = true; }
};

static champsim::modules::instruction_producer::register_module<mock_producer_913>
    mock_producer_reg("MOCK_PRODUCER_913");

// A minimal no-op producer to test default hook behavior
struct noop_producer_913 : public champsim::modules::instruction_producer {
  explicit noop_producer_913(champsim::modules::ModuleBuilder) {}
  const ooo_model_instr* peek() override { return nullptr; }
  void consume() override {}
  [[nodiscard]] bool eof() const override { return true; }
  // No override of hooks — uses defaults
};

static champsim::modules::instruction_producer::register_module<noop_producer_913>
    noop_producer_reg("NOOP_PRODUCER_913");

// Mock core that delegates producers_eof to attached instruction producers
struct mock_core_913 : public champsim::modules::core_module {
  std::vector<champsim::modules::instruction_producer*> producers_;
  std::vector<ooo_model_instr> received_;

  explicit mock_core_913(champsim::modules::ModuleBuilder builder)
    : core_module(champsim::chrono::picoseconds{250})
  {
    for (const auto& sub : builder.get_submodules("instruction_producer", true))
      producers_.push_back(dynamic_cast<champsim::modules::instruction_producer*>(
          champsim::modules::instruction_producer::create_instance(sub, static_cast<champsim::modules::packet_consumer*>(this))));
  }

  void push_instruction(ooo_model_instr instr) override { received_.push_back(instr); }
  std::size_t instructions_requested() override { return 4; }
  uint64_t sim_instr() const override { return 0; }
  uint64_t sim_cycle() const override { return 0; }
  long operate() override { return 0; }
  cpu_stats get_sim_stats() const override { return {}; }

  bool producers_eof() const override {
    return std::all_of(producers_.begin(), producers_.end(),
                       [](const auto* s) { return s->eof(); });
  }

  void fill_from_producers() {
    for (auto* src : producers_) {
      for (auto space = instructions_requested(); space > 0; --space) {
        auto instr = src->next();
        if (!instr.has_value())
          break;
        push_instruction(*instr);
      }
    }
  }
};

static champsim::modules::core_module::register_module<mock_core_913>
    mock_core_reg("MOCK_CORE_913");

// Helper environment for creating modules
struct mock_env_913 : public champsim::modules::environment_module {
  explicit mock_env_913(champsim::modules::ModuleBuilder) {}
  std::vector<std::any> view(const std::string&) const override { return {}; }
  const champsim::modules::ModuleBuilder get_builder_params(const std::string&) const override {
    return champsim::modules::ModuleBuilder();
  }
};

static champsim::modules::environment_module::register_module<mock_env_913>
    mock_env_reg("MOCK_ENV_913");

} // anonymous namespace

TEST_CASE("instruction_producer next() returns queued instructions") {
  auto env_b = champsim::modules::ModuleBuilder("env_ws", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_ws", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_ws", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  REQUIRE(mc != nullptr);
  REQUIRE(mc->producers_.size() == 1);

  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());
  REQUIRE(src != nullptr);

  auto i1 = champsim::test::instruction_with_ip(0xDEAD);
  auto i2 = champsim::test::instruction_with_ip(0xBEEF);
  src->instructions_ = {i1, i2};

  REQUIRE_FALSE(src->eof());
  auto got1 = src->next();
  REQUIRE(got1.has_value());
  CHECK(got1->ip == champsim::address{0xDEAD});
  auto got2 = src->next();
  REQUIRE(got2.has_value());
  CHECK(got2->ip == champsim::address{0xBEEF});
  REQUIRE(src->eof());
  // Exhausted producer reports emptiness instead of invoking UB
  REQUIRE_FALSE(src->next().has_value());
}

TEST_CASE("instruction_producer peek does not consume") {
  auto env_b = champsim::modules::ModuleBuilder("env_peek", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_peek", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_peek", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  src->instructions_.push_back(champsim::test::instruction_with_ip(0xAB));

  // Repeated peeks see the same packet without consuming it
  const auto* p1 = src->peek();
  REQUIRE(p1 != nullptr);
  CHECK(p1->ip == champsim::address{0xAB});
  const auto* p2 = src->peek();
  REQUIRE(p2 == p1);
  REQUIRE_FALSE(src->eof());

  // consume() discards the peeked packet
  src->consume();
  REQUIRE(src->eof());
  REQUIRE(src->peek() == nullptr);
}

TEST_CASE("instruction_producer eof is true when no instructions remain") {
  auto env_b = champsim::modules::ModuleBuilder("env_eof", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_eof", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producers_eof", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  // Empty producer is at EOF
  REQUIRE(src->eof());
  // Add one instruction, no longer at EOF
  src->instructions_.push_back(champsim::test::instruction_with_ip(1));
  REQUIRE_FALSE(src->eof());
  src->next();
  REQUIRE(src->eof());
}

TEST_CASE("core fill_from_producers pulls instructions into core") {
  auto env_b = champsim::modules::ModuleBuilder("env_fill", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_fill", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_fill", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  // Enqueue 3 instructions
  for (int i = 0; i < 3; ++i)
    src->instructions_.push_back(champsim::test::instruction_with_ip(0x1000 + i));

  mc->fill_from_producers();

  REQUIRE(mc->received_.size() == 3);
  CHECK(mc->received_[0].ip == champsim::address{0x1000});
  CHECK(mc->received_[1].ip == champsim::address{0x1001});
  CHECK(mc->received_[2].ip == champsim::address{0x1002});
  REQUIRE(src->eof());
}

TEST_CASE("core producers_eof reflects producer state") {
  auto env_b = champsim::modules::ModuleBuilder("env_seof", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_seof", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_seof", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  // Empty → producers_eof true
  REQUIRE(mc->producers_eof());

  // Add instruction → producers_eof false
  src->instructions_.push_back(champsim::test::instruction_with_ip(1));
  REQUIRE_FALSE(mc->producers_eof());

  // Consume → producers_eof true again
  src->next();
  REQUIRE(mc->producers_eof());
}

TEST_CASE("core with no instruction producers has producers_eof true") {
  auto env_b = champsim::modules::ModuleBuilder("env_nosrc", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_nosrc", "MOCK_CORE_913");
  // No instruction_producer submodule added
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);

  REQUIRE(mc->producers_.empty());
  REQUIRE(mc->producers_eof());
}

TEST_CASE("execution-driven hooks are callable without crash") {
  auto env_b = champsim::modules::ModuleBuilder("env_hooks", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_hooks", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_hooks", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  auto instr = champsim::test::instruction_with_ip(0x42);

  REQUIRE_FALSE(src->retire_called_);
  src->retire_instruction(instr);
  REQUIRE(src->retire_called_);

  REQUIRE_FALSE(src->squash_called_);
  src->squash_instruction(instr);
  REQUIRE(src->squash_called_);

  REQUIRE_FALSE(src->branch_mispredict_called_);
  src->branch_mispredict(instr);
  REQUIRE(src->branch_mispredict_called_);
}

TEST_CASE("default execution-driven hooks are no-ops") {
  auto env_b = champsim::modules::ModuleBuilder("env_noop", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_noop", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_noop", "NOOP_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = mc->producers_.front();

  auto instr = champsim::test::instruction_with_ip(0);
  // These should not crash — default implementations are no-ops
  REQUIRE_NOTHROW(src->retire_instruction(instr));
  REQUIRE_NOTHROW(src->squash_instruction(instr));
  REQUIRE_NOTHROW(src->branch_mispredict(instr));
  // Producer with no instructions is also at EOF
  REQUIRE(src->eof());
}

TEST_CASE("fill_from_producers respects bandwidth limit") {
  auto env_b = champsim::modules::ModuleBuilder("env_bw", "MOCK_ENV_913");
  auto* env = champsim::modules::environment_module::create_instance(env_b, static_cast<champsim::modules::environment_module*>(nullptr));

  auto core_b = champsim::modules::ModuleBuilder("core_bw", "MOCK_CORE_913");
  auto src_b = champsim::modules::ModuleBuilder("producer_bw", "MOCK_PRODUCER_913");
  core_b.add_submodule("instruction_producer", std::move(src_b));
  auto* core = champsim::modules::core_module::create_instance(core_b, env);
  auto* mc = dynamic_cast<mock_core_913*>(core);
  auto* src = dynamic_cast<mock_producer_913*>(mc->producers_.front());

  // Enqueue more instructions than mock bandwidth allows (instructions_requested returns 4)
  for (int i = 0; i < 10; ++i)
    src->instructions_.push_back(champsim::test::instruction_with_ip(i));

  mc->fill_from_producers();

  // Should only pull 4 (the bandwidth limit)
  REQUIRE(mc->received_.size() == 4);
  // 6 should remain in the producer
  REQUIRE(src->instructions_.size() == 6);
  REQUIRE_FALSE(src->eof());
}
