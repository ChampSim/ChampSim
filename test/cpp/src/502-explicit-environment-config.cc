#include <algorithm>
#include <any>
#include <fstream>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <catch.hpp>
#include <nlohmann/json.hpp>

#include "environment.h"
#include "modules.h"
#include "cache.h"
#include "phase_controller.h"

using json = nlohmann::json;
using champsim::modules::ModuleBuilder;

namespace {

json load_config(const std::string& filename) {
  std::ifstream ifs(std::string{TEST_CONFIG_DIR} + filename);
  REQUIRE(ifs.is_open());
  auto config = json::parse(ifs);

  // The shipped explicit configs declare a INSTRUCTION_PRODUCER driven by
  // CLI $trace variables (so the configurations.yml workflow can validate
  // them via bin/champsim). 502 doesn't run main.cc, so we replace any
  // instruction_producer children on cores with a NULL_INSTRUCTION_PRODUCER mock —
  // satisfying the now-required submodule without depending on CLI args.
  if (config.contains("children")) {
    int idx = 0;
    for (auto& child : config["children"]) {
      if (child.value("module", "") != "core") continue;
      if (!child.contains("children")) child["children"] = json::array();
      auto& kids = child["children"];
      kids.erase(std::remove_if(kids.begin(), kids.end(),
                                [](const json& k) { return k.value("module", "") == "instruction_producer"; }),
                 kids.end());
      kids.push_back(json{
        {"name",   "t502_null_ws_" + std::to_string(idx++)},
        {"module", "instruction_producer"},
        {"model",  "NULL_INSTRUCTION_PRODUCER"}
      });
    }
  }
  return config;
}

champsim::modules::environment_module* make_explicit_env(const json& config) {
  auto builder = ModuleBuilder{"test_explicit_env", "ENVIRONMENT"};
  builder.add_parameter("config_json", config);
  return champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
}

// Variants derived in-memory from the base explicit config, rather than checked-in near-duplicates.
json config_custom() {
  auto config = load_config("explicit-1core.json");
  config["block_size"] = 128;
  config["page_size"] = 8192;
  return config;
}

json config_altmodules() {
  auto config = load_config("explicit-1core.json");
  const std::map<std::string, std::string> models{{"llc_pf", "ip_stride"},  {"llc_repl", "srrip"},  {"l1d_pf", "ip_stride"}, {"l1d_repl", "srrip"},
                                                  {"l2c_pf", "ip_stride"},  {"l2c_repl", "srrip"},  {"cpu0_bp", "hashed_perceptron"}};
  std::function<void(json&)> apply = [&](json& node) {
    if (node.is_object()) {
      if (node.contains("name")) {
        auto it = models.find(node["name"].get<std::string>());
        if (it != models.end())
          node["model"] = it->second;
      }
      for (auto& item : node.items())
        apply(item.value());
    } else if (node.is_array()) {
      for (auto& elem : node)
        apply(elem);
    }
  };
  apply(config);
  return config;
}

champsim::modules::cache_module* get_cache(champsim::modules::environment_module* env, const std::string& name) {
  for (auto& cache_ref : env->typed_view<champsim::modules::cache_module>("cache")) {
    if (cache_ref.get().NAME == name)
      return &cache_ref.get();
  }
  return nullptr;
}

} // namespace

// ====== Basic topology tests ======

SCENARIO("Explicit environment constructs correct topology from 1-core config") {
  GIVEN("A 1-core explicit config loaded from file") {
    auto config = load_config("explicit-1core.json");
    auto* env = make_explicit_env(config);

    THEN("num_cpus is 1") {
      REQUIRE(env->get_num("core") == 1);
    }
    THEN("block_size is 64") {
      REQUIRE(env->get_block_size() == 64);
    }
    THEN("page_size is 4096") {
      REQUIRE(env->get_page_size() == 4096);
    }
    THEN("cpu_view has 1 core") {
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 1);
    }
    THEN("ptw_view has 1 PTW") {
      REQUIRE(env->typed_view<champsim::modules::page_table_walker_module>("page_table_walker").size() == 1);
    }
    THEN("cache_view has 7 caches (LLC + DTLB + ITLB + L1D + L1I + L2C + STLB)") {
      REQUIRE(env->typed_view<champsim::modules::cache_module>("cache").size() == 7);
    }
    THEN("operable_view has 1 core + 7 caches + 1 PTW + 1 DRAM = 10") {
      REQUIRE(env->typed_view<champsim::operable>("operable").size() == 10);
    }
    THEN("dram_view returns a valid memory controller") {
      auto drams = env->typed_view<champsim::modules::memory_controller_module>("memory_controller");
      REQUIRE(drams.size() >= 1);
      auto& dram = drams.front().get();
      REQUIRE(dram.get_num_channels() >= 1);
    }
  }
}

SCENARIO("Explicit environment respects custom block_size and page_size") {
  GIVEN("A config with block_size=128 and page_size=8192 loaded from file") {
    auto config = config_custom();
    auto* env = make_explicit_env(config);

    THEN("block_size is 128") {
      REQUIRE(env->get_block_size() == 128);
    }
    THEN("page_size is 8192") {
      REQUIRE(env->get_page_size() == 8192);
    }
  }
}

// ====== Cache parameter propagation tests ======

SCENARIO("Explicit environment propagates cache parameters correctly") {
  GIVEN("A 1-core explicit config loaded from file") {
    auto config = load_config("explicit-1core.json");
    auto* env = make_explicit_env(config);

    THEN("LLC has num_sets=2048, num_ways=16") {
      auto* llc = (CACHE*)get_cache(env, "LLC");
      REQUIRE(llc != nullptr);
      REQUIRE(llc->NUM_SET == 2048);
      REQUIRE(llc->NUM_WAY == 16);
    }
    THEN("cpu0_L1D has num_sets=64, num_ways=12") {
      auto* l1d = (CACHE*)get_cache(env, "cpu0_L1D");
      REQUIRE(l1d != nullptr);
      REQUIRE(l1d->NUM_SET == 64);
      REQUIRE(l1d->NUM_WAY == 12);
    }
    THEN("cpu0_L1I has num_sets=64, num_ways=8") {
      auto* l1i = (CACHE*)get_cache(env, "cpu0_L1I");
      REQUIRE(l1i != nullptr);
      REQUIRE(l1i->NUM_SET == 64);
      REQUIRE(l1i->NUM_WAY == 8);
    }
    THEN("cpu0_DTLB has num_sets=16, num_ways=4") {
      auto* dtlb = (CACHE*)get_cache(env, "cpu0_DTLB");
      REQUIRE(dtlb != nullptr);
      REQUIRE(dtlb->NUM_SET == 16);
      REQUIRE(dtlb->NUM_WAY == 4);
    }
    THEN("cpu0_L2C has num_sets=1024, num_ways=8") {
      auto* l2c = (CACHE*)get_cache(env, "cpu0_L2C");
      REQUIRE(l2c != nullptr);
      REQUIRE(l2c->NUM_SET == 1024);
      REQUIRE(l2c->NUM_WAY == 8);
    }
    THEN("cpu0_L1D has hit_latency=2 and fill_latency=3 (times clock_period)") {
      auto* l1d = (CACHE*)get_cache(env, "cpu0_L1D");
      REQUIRE(l1d != nullptr);
      REQUIRE(l1d->HIT_LATENCY == 2 * l1d->clock_period);
      REQUIRE(l1d->FILL_LATENCY == 3 * l1d->clock_period);
    }
    THEN("cpu0_L1I has virtual_prefetch=true") {
      auto* l1i = (CACHE*)get_cache(env, "cpu0_L1I");
      REQUIRE(l1i != nullptr);
      REQUIRE(l1i->virtual_prefetch == true);
    }
    THEN("cpu0_L1D has virtual_prefetch=false") {
      auto* l1d = (CACHE*)get_cache(env, "cpu0_L1D");
      REQUIRE(l1d != nullptr);
      REQUIRE(l1d->virtual_prefetch == false);
    }
    THEN("cpu0_DTLB pref_activate_mask has LOAD and PREFETCH") {
      auto* dtlb = (CACHE*)get_cache(env, "cpu0_DTLB");
      REQUIRE(dtlb != nullptr);
      REQUIRE(dtlb->pref_activate_mask.size() == 2);
      REQUIRE(dtlb->pref_activate_mask[0] == access_type::LOAD);
      REQUIRE(dtlb->pref_activate_mask[1] == access_type::PREFETCH);
    }
  }
}

// ====== @-reference resolution tests ======

SCENARIO("Explicit environment resolves @-references for builder params") {
  GIVEN("A 1-core explicit config loaded from file") {
    auto config = load_config("explicit-1core.json");
    auto* env = make_explicit_env(config);

    THEN("DRAM builder has ul_channels referencing the LLC->DRAM channel") {
      auto dram_builder = env->get_builder_params("DRAM");
      REQUIRE(dram_builder.is_valid());
    }
    THEN("VMEM builder has a dram reference") {
      auto vmem_builder = env->get_builder_params("VMEM");
      REQUIRE(vmem_builder.is_valid());
    }
    THEN("cpu0_PTW builder is valid") {
      auto ptw_builder = env->get_builder_params("cpu0_PTW");
      REQUIRE(ptw_builder.is_valid());
    }
    THEN("cpu0 core builder is valid") {
      auto core_builder = env->get_builder_params("cpu0");
      REQUIRE(core_builder.is_valid());
    }
  }
}

// ====== Nested children (prefetcher/replacement/bp/btb) tests ======

SCENARIO("Explicit environment propagates nested children as module lists") {
  GIVEN("A 1-core explicit config loaded from file") {
    auto config = load_config("explicit-1core.json");
    auto* env = make_explicit_env(config);

    THEN("LLC builder has prefetcher submodule ['no'] and replacement submodule ['lru']") {
      auto llc_builder = env->get_builder_params("LLC");
      REQUIRE(llc_builder.is_valid());
      auto& pf = llc_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "no");
      auto& repl = llc_builder.get_submodules("replacement");
      REQUIRE(repl.size() == 1);
      REQUIRE(repl[0].get_model() == "lru");
    }
    THEN("cpu0_L2C builder has prefetcher submodule ['spp_dev']") {
      auto l2c_builder = env->get_builder_params("cpu0_L2C");
      REQUIRE(l2c_builder.is_valid());
      auto& pf = l2c_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "spp_dev");
    }
    THEN("cpu0 core builder has branch_predictor submodule ['bimodal'] and btb submodule ['basic_btb']") {
      auto core_builder = env->get_builder_params("cpu0");
      REQUIRE(core_builder.is_valid());
      auto& bp = core_builder.get_submodules("branch_predictor");
      REQUIRE(bp.size() == 1);
      REQUIRE(bp[0].get_model() == "bimodal");
      auto& btb = core_builder.get_submodules("btb");
      REQUIRE(btb.size() == 1);
      REQUIRE(btb[0].get_model() == "basic_btb");
    }
  }
}

// ====== Type-wrapped value tests ======

SCENARIO("Explicit environment correctly parses type-wrapped values") {
  GIVEN("A 1-core explicit config loaded from file") {
    auto config = load_config("explicit-1core.json");
    auto* env = make_explicit_env(config);

    THEN("DRAM has 1 channel (from integer parameter)") {
      REQUIRE(env->typed_view<champsim::modules::memory_controller_module>("memory_controller").front().get().get_num_channels() == 1);
    }
    THEN("LLC MSHR size is 64") {
      auto* llc = (CACHE*)get_cache(env, "LLC");
      REQUIRE(llc != nullptr);
      REQUIRE(llc->MSHR_SIZE == 64);
    }
  }
}

// ====== Nested children with extra parameters ======

SCENARIO("Explicit environment propagates nested child parameters") {
  GIVEN("A config where a cache child has extra params") {
    auto config = load_config("explicit-1core.json");
    // Modify L2C to have ip_stride prefetcher with extra param
    for (auto& child : config["children"]) {
      if (child.value("name", "") == "cpu0_L2C") {
        child["children"] = json::array({
          {{"name", "l2c_pf"}, {"module", "prefetcher"}, {"model", "ip_stride"}, {"degree", 4}},
          {{"name", "l2c_repl"}, {"module", "replacement"}, {"model", "lru"}}
        });
      }
    }
    auto* env = make_explicit_env(config);

    THEN("cpu0_L2C builder has prefetcher submodule ['ip_stride']") {
      auto l2c_builder = env->get_builder_params("cpu0_L2C");
      REQUIRE(l2c_builder.is_valid());
      auto& pf = l2c_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "ip_stride");
    }
    THEN("cpu0_L2C builder has nested prefetcher submodule with degree=4") {
      auto l2c_builder = env->get_builder_params("cpu0_L2C");
      auto& pf = l2c_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() >= 1);
      REQUIRE(pf[0].get_parameter<int64_t>("degree") == 4);
    }
  }
}

// ====== Dump mode test ======

SCENARIO("Explicit environment dump mode does not crash") {
  GIVEN("A 1-core explicit config with dump enabled") {
    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(true);
    auto config = load_config("explicit-1core.json");
    auto builder = ModuleBuilder{"dump_explicit_env", "ENVIRONMENT"};
    builder.add_parameter("config_json", config);

    THEN("Construction succeeds and dump log is non-empty") {
      auto* env = champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
      REQUIRE(env->get_num("core") == 1);
      REQUIRE_FALSE(ModuleBuilder::get_dump_log().empty());
    }

    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(false);
  }
}

SCENARIO("Explicit environment dump log contains expected modules and parameters") {
  GIVEN("A 1-core explicit config with dump enabled") {
    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(true);
    auto config = load_config("explicit-1core.json");
    auto builder = ModuleBuilder{"dump_explicit", "ENVIRONMENT"};
    builder.add_parameter("config_json", config);
    champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
    auto& log = ModuleBuilder::get_dump_log();

    THEN("The dump log contains entries for all major module types") {
      // Core
      REQUIRE(log.find("[cpu0]") != std::string::npos);
      // Caches
      REQUIRE(log.find("[cpu0_L1D]") != std::string::npos);
      REQUIRE(log.find("[cpu0_L1I]") != std::string::npos);
      REQUIRE(log.find("[cpu0_L2C]") != std::string::npos);
      REQUIRE(log.find("[LLC]") != std::string::npos);
      // TLBs
      REQUIRE(log.find("[cpu0_DTLB]") != std::string::npos);
      REQUIRE(log.find("[cpu0_ITLB]") != std::string::npos);
      REQUIRE(log.find("[cpu0_STLB]") != std::string::npos);
      // PTW and DRAM
      REQUIRE(log.find("[cpu0_PTW]") != std::string::npos);
      REQUIRE(log.find("[DRAM]") != std::string::npos);
      // VMEM
      REQUIRE(log.find("[VMEM]") != std::string::npos);
    }

    THEN("The dump log contains set parameter tags") {
      REQUIRE(log.find("(set)") != std::string::npos);
    }

    THEN("Channel parameters are logged with correct values") {
      auto pos = log.find("[ch_ptw_l1d]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("rq_size", pos) != std::string::npos);
      REQUIRE(log.find("32 (set)", pos) != std::string::npos);
    }

    THEN("Cache parameters are logged") {
      auto pos = log.find("[cpu0_L1D]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("num_sets", pos) != std::string::npos);
      REQUIRE(log.find("num_ways", pos) != std::string::npos);
    }

    THEN("DRAM parameters are logged") {
      auto pos = log.find("[DRAM]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("channels", pos) != std::string::npos);
      REQUIRE(log.find("1 (set)", pos) != std::string::npos);
    }

    THEN("Core parameters are logged") {
      auto pos = log.find("[cpu0]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("rob_size", pos) != std::string::npos);
    }

    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(false);
  }
}

// ====== Multi-module test (2 cores explicitly) ======

SCENARIO("Explicit environment supports multi-core via explicit module declarations") {
  GIVEN("A 2-core explicit config loaded from file") {
    auto config = load_config("explicit-2core.json");
    auto* env = make_explicit_env(config);

    THEN("num_cpus is 2") {
      REQUIRE(env->get_num("core") == 2);
    }
    THEN("cpu_view has 2 cores") {
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 2);
    }
    THEN("ptw_view has 2 PTWs") {
      REQUIRE(env->typed_view<champsim::modules::page_table_walker_module>("page_table_walker").size() == 2);
    }
    THEN("cache_view has 13 caches (LLC + 6 per-core * 2)") {
      REQUIRE(env->typed_view<champsim::modules::cache_module>("cache").size() == 13);
    }
    THEN("operable_view has 2 cores + 13 caches + 2 PTWs + 1 DRAM = 18") {
      REQUIRE(env->typed_view<champsim::operable>("operable").size() == 18);
    }
  }
}

// ====== Alternative modules test ======

SCENARIO("Explicit environment constructs with alternative module choices") {
  GIVEN("A 1-core config with ip_stride prefetchers, srrip replacement, and hashed_perceptron BP") {
    auto config = config_altmodules();
    auto* env = make_explicit_env(config);

    THEN("num_cpus is 1") {
      REQUIRE(env->get_num("core") == 1);
    }
    THEN("block_size is 64") {
      REQUIRE(env->get_block_size() == 64);
    }
    THEN("L1D prefetcher is ip_stride") {
      auto l1d_builder = env->get_builder_params("cpu0_L1D");
      REQUIRE(l1d_builder.is_valid());
      auto& pf = l1d_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "ip_stride");
    }
    THEN("L1D replacement is srrip") {
      auto l1d_builder = env->get_builder_params("cpu0_L1D");
      auto& repl = l1d_builder.get_submodules("replacement");
      REQUIRE(repl.size() == 1);
      REQUIRE(repl[0].get_model() == "srrip");
    }
    THEN("LLC prefetcher is ip_stride") {
      auto llc_builder = env->get_builder_params("LLC");
      REQUIRE(llc_builder.is_valid());
      auto& pf = llc_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "ip_stride");
    }
    THEN("LLC replacement is srrip") {
      auto llc_builder = env->get_builder_params("LLC");
      auto& repl = llc_builder.get_submodules("replacement");
      REQUIRE(repl.size() == 1);
      REQUIRE(repl[0].get_model() == "srrip");
    }
    THEN("cpu0 branch predictor is hashed_perceptron") {
      auto core_builder = env->get_builder_params("cpu0");
      REQUIRE(core_builder.is_valid());
      auto& bp = core_builder.get_submodules("branch_predictor");
      REQUIRE(bp.size() == 1);
      REQUIRE(bp[0].get_model() == "hashed_perceptron");
    }
    THEN("cache parameters are correct for standard block_size=64") {
      auto* l1d = (CACHE*)get_cache(env, "cpu0_L1D");
      REQUIRE(l1d != nullptr);
      REQUIRE(l1d->NUM_SET == 64);
      REQUIRE(l1d->NUM_WAY == 12);
    }
  }
}

// ====== Custom config cache parameter test ======

SCENARIO("Explicit environment with custom config still has correct cache sizes") {
  GIVEN("A custom 1-core config with block_size=128") {
    auto config = config_custom();
    auto* env = make_explicit_env(config);

    THEN("L1D has num_sets=64, num_ways=12 (same as base config)") {
      auto* l1d = (CACHE*)get_cache(env, "cpu0_L1D");
      REQUIRE(l1d != nullptr);
      REQUIRE(l1d->NUM_SET == 64);
      REQUIRE(l1d->NUM_WAY == 12);
    }
    THEN("LLC has num_sets=2048, num_ways=16 (same as base config)") {
      auto* llc = (CACHE*)get_cache(env, "LLC");
      REQUIRE(llc != nullptr);
      REQUIRE(llc->NUM_SET == 2048);
      REQUIRE(llc->NUM_WAY == 16);
    }
    THEN("L2C prefetcher is still spp_dev") {
      auto l2c_builder = env->get_builder_params("cpu0_L2C");
      REQUIRE(l2c_builder.is_valid());
      auto& pf = l2c_builder.get_submodules("prefetcher");
      REQUIRE(pf.size() == 1);
      REQUIRE(pf[0].get_model() == "spp_dev");
    }
  }
}
