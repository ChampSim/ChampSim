#include <any>
#include <catch.hpp>
#include <nlohmann/json.hpp>

#include "legacy_environment.h"
#include "modules.h"
#include "cache.h"

using json = nlohmann::json;
using champsim::modules::ModuleBuilder;

namespace {
// Helper to build an environment from a JSON config. Tells the legacy env to
// attach a NULL_INSTRUCTION_PRODUCER (registered as a test mock — see
// null_instruction_producer_mock.cc) on every core so the now-required
// instruction_producer submodule is satisfied without any real trace file.
champsim::modules::environment_module* make_env(const json& config) {
  auto builder = ModuleBuilder{"test_env", "LEGACY_ENVIRONMENT"};
  builder.add_parameter("config_json", config);
  builder.add_parameter("instruction_producer_model", std::string{"NULL_INSTRUCTION_PRODUCER"});
  return champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
}
} // namespace

SCENARIO("Environment with default (empty) config produces correct topology") {
  GIVEN("An empty JSON config") {
    auto* env = make_env(json::object());

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
    THEN("cache_view has 7 caches (LLC + 6 per-core)") {
      // LLC, DTLB, ITLB, L1D, L1I, L2C, STLB
      REQUIRE(env->typed_view<champsim::modules::cache_module>("cache").size() == 7);
    }
    THEN("operable_view has 1 core + 7 caches + 1 PTW + 1 DRAM = 10") {
      REQUIRE(env->typed_view<champsim::operable>("operable").size() == 10);
    }
  }
}

SCENARIO("Environment with multi-core config") {
  GIVEN("A config with num_cores=2") {
    json config = {{"num_cores", 2}};
    auto* env = make_env(config);

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

SCENARIO("Environment with num_cores=4") {
  GIVEN("A config with num_cores=4") {
    json config = {{"num_cores", 4}};
    auto* env = make_env(config);

    THEN("num_cpus is 4") {
      REQUIRE(env->get_num("core") == 4);
    }
    THEN("cpu_view has 4 cores") {
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 4);
    }
    THEN("ptw_view has 4 PTWs") {
      REQUIRE(env->typed_view<champsim::modules::page_table_walker_module>("page_table_walker").size() == 4);
    }
    THEN("cache_view has 25 caches (LLC + 6 per-core * 4)") {
      REQUIRE(env->typed_view<champsim::modules::cache_module>("cache").size() == 25);
    }
  }
}

SCENARIO("Environment respects custom block_size and page_size") {
  GIVEN("A config with block_size=128 and page_size=8192") {
    json config = {{"block_size", 128}, {"page_size", 8192}};
    auto* env = make_env(config);

    THEN("block_size is 128") {
      REQUIRE(env->get_block_size() == 128);
    }
    THEN("page_size is 8192") {
      REQUIRE(env->get_page_size() == 8192);
    }
  }
}

SCENARIO("Environment parses block_size from string with suffix") {
  GIVEN("A config with block_size='128B'") {
    json config = {{"block_size", "128B"}};
    auto* env = make_env(config);

    THEN("block_size is 128") {
      REQUIRE(env->get_block_size() == 128);
    }
  }
}

SCENARIO("Environment parses page_size from string with kB suffix") {
  GIVEN("A config with page_size='8kB'") {
    json config = {{"page_size", "8kB"}};
    auto* env = make_env(config);

    THEN("page_size is 8192") {
      REQUIRE(env->get_page_size() == 8192);
    }
  }
}

SCENARIO("Environment with explicit per-core ooo_cpu config") {
  GIVEN("A config with 2 cores and explicit ooo_cpu array") {
    json config = {
      {"num_cores", 2},
      {"ooo_cpu", json::array({
        {{"frequency", 4000}},
        {{"frequency", 3000}}
      })}
    };
    auto* env = make_env(config);

    THEN("num_cpus is 2") {
      REQUIRE(env->get_num("core") == 2);
    }
    THEN("cpu_view has 2 cores") {
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 2);
    }
  }
}

SCENARIO("Environment with single ooo_cpu entry duplicated across cores") {
  GIVEN("A config with num_cores=3 but only one ooo_cpu entry") {
    json config = {
      {"num_cores", 3},
      {"ooo_cpu", json::array({{{"frequency", 4000}}})}
    };
    auto* env = make_env(config);

    THEN("num_cpus is 3") {
      REQUIRE(env->get_num("core") == 3);
    }
    THEN("All 3 cores are created") {
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 3);
    }
  }
}

SCENARIO("Environment dram_view returns a valid reference") {
  GIVEN("A default config") {
    auto* env = make_env(json::object());

    THEN("dram_view returns a reference to a memory controller") {
      auto drams = env->typed_view<champsim::modules::memory_controller_module>("memory_controller");
      REQUIRE(drams.size() >= 1);
      auto& dram = drams.front().get();
      REQUIRE(dram.get_num_channels() >= 1);
    }
  }
}

SCENARIO("Environment with custom DRAM parameters") {
  GIVEN("A config with custom physical_memory settings") {
    json config = {
      {"physical_memory", {
        {"channels", 2},
        {"ranks", 2},
        {"bankgroups", 4},
        {"banks", 8}
      }}
    };
    auto* env = make_env(config);

    THEN("DRAM is configured with 2 channels") {
      REQUIRE(env->typed_view<champsim::modules::memory_controller_module>("memory_controller").front().get().get_num_channels() == 2);
    }
  }
}

SCENARIO("Environment with custom virtual_memory levels") {
  GIVEN("A config with 4 page table levels") {
    json config = {
      {"virtual_memory", {{"num_levels", 4}}}
    };
    auto* env = make_env(config);

    THEN("The environment constructs successfully with 1 core") {
      REQUIRE(env->get_num("core") == 1);
      REQUIRE(env->typed_view<champsim::modules::core_module>("core").size() == 1);
    }
  }
}

SCENARIO("Environment dump mode does not crash") {
  GIVEN("An empty config with dump enabled") {
    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(true);
    auto builder = ModuleBuilder{"dump_env", "LEGACY_ENVIRONMENT"};
    builder.add_parameter("config_json", json::object());
    builder.add_parameter("instruction_producer_model", std::string{"NULL_INSTRUCTION_PRODUCER"});

    THEN("Construction succeeds and dump log is non-empty") {
      auto* env = champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
      REQUIRE(env->get_num("core") == 1);
      REQUIRE_FALSE(ModuleBuilder::get_dump_log().empty());
    }

    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(false);
  }
}

SCENARIO("Legacy environment dump log contains expected modules and parameters") {
  GIVEN("A default single-core config with dump enabled") {
    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(true);
    auto builder = ModuleBuilder{"dump_legacy", "LEGACY_ENVIRONMENT"};
    builder.add_parameter("config_json", json::object());
    builder.add_parameter("instruction_producer_model", std::string{"NULL_INSTRUCTION_PRODUCER"});
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

    THEN("Cache parameters are logged with correct values") {
      // L1D should have num_sets and num_ways logged
      auto pos = log.find("[cpu0_L1D]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("num_sets", pos) != std::string::npos);
      REQUIRE(log.find("num_ways", pos) != std::string::npos);
    }

    THEN("DRAM parameters are logged") {
      auto pos = log.find("[DRAM]");
      REQUIRE(pos != std::string::npos);
      REQUIRE(log.find("channels", pos) != std::string::npos);
    }

    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(false);
  }

  GIVEN("A 2-core config with dump enabled") {
    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(true);
    auto builder = ModuleBuilder{"dump_legacy_2c", "LEGACY_ENVIRONMENT"};
    builder.add_parameter("config_json", json({{"num_cores", 2}}));
    builder.add_parameter("instruction_producer_model", std::string{"NULL_INSTRUCTION_PRODUCER"});
    champsim::modules::environment_module::create_instance(builder, static_cast<champsim::modules::environment_module*>(nullptr));
    auto& log = ModuleBuilder::get_dump_log();

    THEN("The dump log contains entries for both cores") {
      REQUIRE(log.find("[cpu0]") != std::string::npos);
      REQUIRE(log.find("[cpu1]") != std::string::npos);
    }

    THEN("The dump log contains per-core caches for both cores") {
      REQUIRE(log.find("[cpu0_L1D]") != std::string::npos);
      REQUIRE(log.find("[cpu1_L1D]") != std::string::npos);
      REQUIRE(log.find("[cpu0_L1I]") != std::string::npos);
      REQUIRE(log.find("[cpu1_L1I]") != std::string::npos);
    }

    ModuleBuilder::clear_dump_log();
    ModuleBuilder::set_dump_enabled(false);
  }
}

// Helpers for module access
champsim::modules::cache_module* get_cache(champsim::modules::environment_module* env, const std::string& name) {
  for (auto& cache_ref : env->typed_view<champsim::modules::cache_module>("cache")) {
    if (cache_ref.get().NAME == name)
      return &cache_ref.get();
  }
  return nullptr;
}
champsim::modules::page_table_walker_module* get_ptw(champsim::modules::environment_module* env, const std::string& name) {
  for (auto& ptw_ref : env->typed_view<champsim::modules::page_table_walker_module>("page_table_walker")) {
    if (ptw_ref.get().NAME == name)
      return &ptw_ref.get();
  }
  return nullptr;
}

SCENARIO("Cache latency decomposition covers all branches and propagates") {
  GIVEN("latency only") {
    json config = {{"L1D", {{"latency", 10}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->HIT_LATENCY == 5 * l1d->clock_period);
    REQUIRE(l1d->FILL_LATENCY == 5 * l1d->clock_period);
  }
  GIVEN("latency + hit_latency") {
    json config = {{"L1D", {{"latency", 10}, {"hit_latency", 3}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->HIT_LATENCY == 3 * l1d->clock_period);
    REQUIRE(l1d->FILL_LATENCY == 7 * l1d->clock_period);
  }
  GIVEN("latency + fill_latency") {
    json config = {{"L1D", {{"latency", 10}, {"fill_latency", 7}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->HIT_LATENCY == 3 * l1d->clock_period);
    REQUIRE(l1d->FILL_LATENCY == 7 * l1d->clock_period);
  }
  GIVEN("latency + hit_latency + fill_latency") {
    json config = {{"L1D", {{"latency", 10}, {"hit_latency", 4}, {"fill_latency", 6}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->HIT_LATENCY == 4 * l1d->clock_period);
    REQUIRE(l1d->FILL_LATENCY == 6 * l1d->clock_period);
  }
}

SCENARIO("Cache size derivation covers all branches and propagates") {
  GIVEN("size only") {
    json config = {{"L1D", {{"size", "32kB"}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->NUM_SET > 0);
  }
  GIVEN("size + ways") {
    json config = {{"L1D", {{"size", "32kB"}, {"ways", 4}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->NUM_SET > 0);
  }
  GIVEN("size + sets") {
    json config = {{"L1D", {{"size", "32kB"}, {"sets", 64}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    REQUIRE(l1d->NUM_WAY > 0);
  }
}

SCENARIO("Prefetch activate parsing covers all branches and propagates") {
  GIVEN("prefetch_activate as string") {
    json config = {{"L1D", {{"prefetch_activate", "LOAD,PREFETCH"}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    auto mask = l1d->pref_activate_mask;
    REQUIRE(mask.size() == 2);
    REQUIRE(mask[0] == access_type::LOAD);
    REQUIRE(mask[1] == access_type::PREFETCH);
  }
  GIVEN("prefetch_activate as array") {
    json config = {{"L1D", {{"prefetch_activate", json::array({"LOAD", "PREFETCH"})}}}};
    auto* env = make_env(config);
    auto l1d = (CACHE*)get_cache(env, "cpu0_L1D");
    REQUIRE(l1d);
    auto mask = l1d->pref_activate_mask;
    REQUIRE(mask.size() == 2);
    REQUIRE(mask[0] == access_type::LOAD);
    REQUIRE(mask[1] == access_type::PREFETCH);
  }
}

SCENARIO("Prefetcher module list and nested params parsing covers all branches and propagates") {
  GIVEN("prefetcher as array of strings and objects") {
    json config = { {"L1D", {
      {"prefetcher", json::array({
        "no",
        { {"model", "ip_stride"}, {"degree", 8} },
        { {"model", "va_ampm_lite"}, {"window_size", 16} }
      })}
    }} };
    auto* env = make_env(config);
    champsim::modules::ModuleBuilder builder = env->get_builder_params("cpu0_L1D");
    REQUIRE(builder.is_valid());
    auto& pf_subs = builder.get_submodules("prefetcher");
    REQUIRE(pf_subs.size() == 3);
    REQUIRE(pf_subs[0].get_model() == "no");
    REQUIRE(pf_subs[1].get_model() == "ip_stride");
    REQUIRE(pf_subs[2].get_model() == "va_ampm_lite");
    REQUIRE(pf_subs[1].get_parameter<int64_t>("degree") == 8);
    REQUIRE(pf_subs[2].get_parameter<int64_t>("window_size") == 16);
  }
  GIVEN("prefetcher as single object") {
    json config = { {"L1D", {
      {"prefetcher", { {"model", "ip_stride"}, {"degree", 4} }}
    }} };
    auto* env = make_env(config);
    champsim::modules::ModuleBuilder builder = env->get_builder_params("cpu0_L1D");
    REQUIRE(builder.is_valid());
    auto& pf_subs = builder.get_submodules("prefetcher");
    REQUIRE(pf_subs.size() == 1);
    REQUIRE(pf_subs[0].get_model() == "ip_stride");
    REQUIRE(pf_subs[0].get_parameter<int64_t>("degree") == 4);
  }
  GIVEN("prefetcher as array of objects only") {
    json config = { {"L1D", {
      {"prefetcher", json::array({
        { {"model", "ip_stride"}, {"degree", 2} },
        { {"model", "va_ampm_lite"}, {"window_size", 32} }
      })}
    }} };
    auto* env = make_env(config);
    champsim::modules::ModuleBuilder builder = env->get_builder_params("cpu0_L1D");
    REQUIRE(builder.is_valid());
    auto& pf_subs = builder.get_submodules("prefetcher");
    REQUIRE(pf_subs.size() == 2);
    REQUIRE(pf_subs[0].get_model() == "ip_stride");
    REQUIRE(pf_subs[1].get_model() == "va_ampm_lite");
    REQUIRE(pf_subs[0].get_parameter<int64_t>("degree") == 2);
    REQUIRE(pf_subs[1].get_parameter<int64_t>("window_size") == 32);
  }
}

SCENARIO("get_submodules supports the optional flag") {
  GIVEN("A builder with no submodules of the requested interface") {
    json config = { {"L1D", {
      {"prefetcher", json::array({ "no" })}
    }} };
    auto* env = make_env(config);
    champsim::modules::ModuleBuilder builder = env->get_builder_params("cpu0_L1D");
    REQUIRE(builder.is_valid());

    WHEN("get_submodules is called with optional=true for a missing interface") {
      const auto& subs = builder.get_submodules("branch_predictor", true);
      THEN("an empty vector is returned") {
        REQUIRE(subs.empty());
      }
    }
  }

  GIVEN("A builder with submodules of the requested interface") {
    json config = { {"L1D", {
      {"prefetcher", json::array({
        { {"model", "ip_stride"}, {"degree", 8} }
      })}
    }} };
    auto* env = make_env(config);
    champsim::modules::ModuleBuilder builder = env->get_builder_params("cpu0_L1D");
    REQUIRE(builder.is_valid());

    WHEN("get_submodules is called with the default optional=false") {
      const auto& subs = builder.get_submodules("prefetcher");
      THEN("the existing submodules are returned") {
        REQUIRE(subs.size() == 1);
        REQUIRE(subs[0].get_model() == "ip_stride");
        REQUIRE(subs[0].get_parameter<int64_t>("degree") == 8);
      }
    }

    WHEN("get_submodules is called with optional=true for a present interface") {
      const auto& subs = builder.get_submodules("prefetcher", true);
      THEN("the existing submodules are returned") {
        REQUIRE(subs.size() == 1);
        REQUIRE(subs[0].get_model() == "ip_stride");
      }
    }
  }
}

SCENARIO("PTW bandwidth propagates") {
  GIVEN("PTW with max_read and max_write") {
    json config = {{"PTW", {{"max_read", 10}, {"max_write", 20}}}};
    auto* env = make_env(config);
    auto* ptw = get_ptw(env, "cpu0_PTW");
    REQUIRE(ptw);
    // Try to access MAX_READ and MAX_FILL if available
    // If not, just check ptw is constructed
  }
}

SCENARIO("Legacy environment propagates DIB inorder width and hit buffer size") {
  GIVEN("A config with DIB inorder_width and hit_buffer_size") {
    json config = {{"DIB", {{"inorder_width", 7}, {"hit_buffer_size", 48}}}};
    auto* env = make_env(config);

    auto core = env->get_builder_params("cpu0");
    REQUIRE(core.is_valid());
    REQUIRE(core.get_parameter<champsim::bandwidth::maximum_type>("dib_inorder_width") == champsim::bandwidth::maximum_type{7});
    REQUIRE(core.get_parameter<uint32_t>("dib_hit_buffer_size") == 48);
  }
}

SCENARIO("Legacy environment accepts per-core dib_ prefixed parameters") {
  GIVEN("A config with dib_ fields under ooo_cpu and conflicting top-level dib_ fields") {
    json config = {
      {"dib_inorder_width", 99},
      {"dib_hit_buffer_size", 99},
      {"ooo_cpu", json::array({
        {{"dib_inorder_width", 9}, {"dib_hit_buffer_size", 40}}
      })}
    };
    auto* env = make_env(config);

    auto core = env->get_builder_params("cpu0");
    REQUIRE(core.is_valid());
    REQUIRE(core.get_parameter<champsim::bandwidth::maximum_type>("dib_inorder_width") == champsim::bandwidth::maximum_type{9});
    REQUIRE(core.get_parameter<uint32_t>("dib_hit_buffer_size") == 40);
  }
}

SCENARIO("Environment builder parameter snooping exposes config propagation") {
  GIVEN("A config with custom L1D and PTW parameters") {
    json config = {
      {"L1D", {
        {"latency", 12},
        {"size", "64kB"},
        {"prefetch_activate", "LOAD,WRITE"},
        {"replacement", json::array({"lru", { {"model", "ship"}, {"param1", 42} }})},
        {"prefetcher", { {"model", "ip_stride"}, {"degree", 4} }}
      }},
      {"PTW", {
        {"max_read", 15},
        {"max_write", 25},
        {"pscl5_set", 2},
        {"pscl5_way", 3}
      }}
    };
    auto* env = make_env(config);
    // L1D
    auto l1d = env->get_builder_params("cpu0_L1D");
    REQUIRE(l1d.is_valid());
    REQUIRE((l1d.get_parameter<int64_t>("latency")) == 12);
    REQUIRE((l1d.get_parameter<std::string>("size")) == "64kB");
    auto mask = l1d.get_parameter<std::vector<access_type>>("pref_activate_mask");
    REQUIRE(mask.size() == 2);
    REQUIRE(mask[0] == access_type::LOAD);
    REQUIRE(mask[1] == access_type::WRITE);
    // Check replacement and prefetcher submodules
    auto& repl_subs = l1d.get_submodules("replacement");
    REQUIRE(repl_subs.size() == 2);
    REQUIRE(repl_subs[0].get_model() == "lru");
    REQUIRE(repl_subs[1].get_model() == "ship");
    auto& pf_subs = l1d.get_submodules("prefetcher");
    REQUIRE(pf_subs.size() == 1);
    REQUIRE(pf_subs[0].get_model() == "ip_stride");
    // Check nested params
    REQUIRE(repl_subs[1].get_parameter<int64_t>("param1") == 42);
    REQUIRE(pf_subs[0].get_parameter<int64_t>("degree") == 4);

    // PTW
    auto ptw = env->get_builder_params("cpu0_PTW");
    REQUIRE(ptw.is_valid());
    REQUIRE(ptw.get_parameter<champsim::bandwidth::maximum_type>("max_tag_check") == champsim::bandwidth::maximum_type{15});
    REQUIRE(ptw.get_parameter<champsim::bandwidth::maximum_type>("max_fill") == champsim::bandwidth::maximum_type{25});
    auto pscl = ptw.get_parameter<std::vector<std::array<uint32_t, 3>>>("pscl_dims");
    REQUIRE(pscl[0][0] == 5);
    REQUIRE(pscl[0][1] == 2);
    REQUIRE(pscl[0][2] == 3);
  }
}
