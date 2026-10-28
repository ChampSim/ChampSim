#include <catch2/catch_test_macros.hpp>
#include <any>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "cache_stats.h"
#include "core_stats.h"
#include "dram_stats.h"
#include "json_stat_builder.h"
#include "modules.h"
#include "phase_info.h"
#include "stat_report.h"

namespace {

// A test core that publishes stats via the module_lifecycle interface.
struct stats_core : public champsim::modules::core_module {
  cpu_stats sim_stats_{};

  explicit stats_core(champsim::modules::ModuleBuilder)
    : core_module(champsim::chrono::picoseconds{250}) {}

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return 100; }
  uint64_t sim_cycle() const override { return 200; }
  long operate() override { return 0; }
  cpu_stats get_sim_stats() const override { return sim_stats_; }

  void end_phase(champsim::stat_report& out) override { format_stats(sim_stats_, out); }
};

static champsim::modules::core_module::register_module<stats_core> stats_core_reg("STATS_CORE_912");

// A test cache that publishes stats via the module_lifecycle interface.
struct stats_cache : public champsim::modules::cache_module {
  cache_stats sim_stats_{};

  explicit stats_cache(champsim::modules::ModuleBuilder)
    : cache_module(champsim::chrono::picoseconds{250}) {}

  long operate() override { return 0; }
  champsim::bandwidth::maximum_type get_max_tag_bandwidth() const override { return {}; }
  cache_stats get_sim_stats() const override { return sim_stats_; }
  bool is_virtual_prefetch() const override { return false; }
  bool prefetch_line(champsim::address, bool, uint32_t) override { return false; }
  void impl_update_replacement_state(champsim::origin, long, long, champsim::address, champsim::address,
                                     champsim::address, access_type, bool) const override {}
  void impl_prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) const override {}
  long invalidate_entry(champsim::address) override { return -1; }
  std::size_t get_mshr_occupancy() const override { return 0; }
  std::size_t get_mshr_size() const override { return 0; }
  double get_mshr_occupancy_ratio() const override { return 0; }
  std::vector<std::size_t> get_rq_occupancy() const override { return {}; }
  std::vector<std::size_t> get_rq_size() const override { return {}; }
  std::vector<double> get_rq_occupancy_ratio() const override { return {}; }
  std::vector<std::size_t> get_wq_occupancy() const override { return {}; }
  std::vector<std::size_t> get_wq_size() const override { return {}; }
  std::vector<double> get_wq_occupancy_ratio() const override { return {}; }
  std::vector<std::size_t> get_pq_occupancy() const override { return {}; }
  std::vector<std::size_t> get_pq_size() const override { return {}; }
  std::vector<double> get_pq_occupancy_ratio() const override { return {}; }
  std::size_t num_sets() const override { return 0; }
  std::size_t num_ways() const override { return 0; }
  champsim::data::bits get_offset_bits() const override { return champsim::data::bits{}; }

  void end_phase(champsim::stat_report& out) override { format_stats(sim_stats_, out); }
};

static champsim::modules::cache_module::register_module<stats_cache> stats_cache_reg("STATS_CACHE_912");

// Mock environment for stats tests
struct stats_env : public champsim::modules::environment_module {
  std::vector<champsim::modules::core_module*> cores_;
  std::vector<champsim::modules::cache_module*> caches_;

  explicit stats_env(champsim::modules::ModuleBuilder) {}

  std::vector<std::any> view(const std::string& interface_type) const override {
    std::vector<std::any> result;
    if (interface_type == "core") {
      for (auto* c : cores_) result.push_back(static_cast<champsim::modules::core_module*>(c));
    } else if (interface_type == "cache") {
      for (auto* c : caches_) result.push_back(static_cast<champsim::modules::cache_module*>(c));
    }
    return result;
  }

  const champsim::modules::ModuleBuilder get_builder_params(const std::string&) const override {
    return champsim::modules::ModuleBuilder{};
  }
};

} // anonymous namespace

TEST_CASE("module_lifecycle::report_stats fills the report with formatted plaintext") {
  auto builder = champsim::modules::ModuleBuilder("test_core", "STATS_CORE_912");
  stats_env env_dummy(champsim::modules::ModuleBuilder{});
  auto* core = champsim::modules::core_module::create_instance(builder, &env_dummy);

  auto* impl = static_cast<stats_core*>(core);
  impl->sim_stats_.name = "core0";
  impl->sim_stats_.begin_instrs = 0;
  impl->sim_stats_.end_instrs = 100;
  impl->sim_stats_.begin_cycles = 0;
  impl->sim_stats_.end_cycles = 50;

  auto* ms = dynamic_cast<champsim::module_lifecycle*>(core);
  REQUIRE(ms != nullptr);
  champsim::stat_report report;
  ms->end_phase(report);
  REQUIRE(!report.text().empty());
  REQUIRE(report.text().front().find("core0 cumulative IPC") != std::string::npos);
}

TEST_CASE("module_lifecycle::report_stats fills the report's JSON object") {
  auto builder = champsim::modules::ModuleBuilder("test_core_json", "STATS_CORE_912");
  stats_env env_dummy(champsim::modules::ModuleBuilder{});
  auto* core = champsim::modules::core_module::create_instance(builder, &env_dummy);

  auto* impl = static_cast<stats_core*>(core);
  impl->sim_stats_.name = "core0";
  impl->sim_stats_.begin_instrs = 0;
  impl->sim_stats_.end_instrs = 200;
  impl->sim_stats_.begin_cycles = 0;
  impl->sim_stats_.end_cycles = 100;

  auto* ms = dynamic_cast<champsim::module_lifecycle*>(core);
  REQUIRE(ms != nullptr);
  champsim::stat_report report;
  ms->end_phase(report);
  REQUIRE(!report.empty());
  REQUIRE(report.json_object().contains("instructions"));
  REQUIRE(report.json_object().contains("cycles"));
}

TEST_CASE("cache module_lifecycle::report_stats produces lines tagged with the cache name") {
  auto builder = champsim::modules::ModuleBuilder("test_cache_912", "STATS_CACHE_912");
  stats_env env_dummy(champsim::modules::ModuleBuilder{});
  auto* cache = champsim::modules::cache_module::create_instance(builder, &env_dummy);

  auto* impl = static_cast<stats_cache*>(cache);
  impl->sim_stats_.name = "L1D";
  impl->sim_stats_.hits.set({access_type::LOAD, std::size_t{0}}, 100);

  auto* ms = dynamic_cast<champsim::module_lifecycle*>(cache);
  REQUIRE(ms != nullptr);
  champsim::stat_report report;
  ms->end_phase(report);
  REQUIRE(!report.text().empty());
  REQUIRE(report.text().front().find("L1D") != std::string::npos);
}

TEST_CASE("interface_registry exposes module_lifecycle conversions for opted-in interfaces") {
  REQUIRE(champsim::modules::interface_registry::get_to_module_lifecycle("core") != nullptr);
  REQUIRE(champsim::modules::interface_registry::get_to_module_lifecycle("cache") != nullptr);
}

TEST_CASE("interface_registry::identify reports model and name for an instance") {
  auto builder = champsim::modules::ModuleBuilder("identify_core", "STATS_CORE_912");
  stats_env env_dummy(champsim::modules::ModuleBuilder{});
  auto* core = champsim::modules::core_module::create_instance(builder, &env_dummy);
  std::any inst = static_cast<champsim::modules::core_module*>(core);
  auto id = champsim::modules::interface_registry::identify("core", inst);
  REQUIRE(id.model == "STATS_CORE_912");
  REQUIRE(id.name  == "identify_core");
}

TEST_CASE("json_stat_builder produces nested groups and arrays") {
  champsim::json_stat_builder b;
  b.add("count", 42)
   .add("name", std::string{"hello"});
  auto sub = b.group("nested");
  sub.add("inner", 1);
  b.push_back("series", 1).push_back("series", 2).push_back("series", 3);

  REQUIRE(b.json()["count"] == 42);
  REQUIRE(b.json()["name"]  == "hello");
  REQUIRE(b.json()["nested"]["inner"] == 1);
  REQUIRE(b.json()["series"].size() == 3);
}
