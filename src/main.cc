/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include "cache.h" // for CACHE
#include "champsim.h"
#include "defaults.hpp"
#include "environment.h"
#include "legacy_environment.h"
#include "modules.h"
#include "ooo_cpu.h" // for O3_CPU
#include "phase_controller.h"
#include "phase_info.h"
#include "stats_printer.h"
#include "vmem.h"

namespace champsim
{
std::vector<phase_stats> main(modules::environment_module& env);
void assign_identities(modules::environment_module& env);
} // namespace champsim

// Collect all $varname references from a JSON document (recursive).
static void collect_config_vars(const nlohmann::json& node, std::set<std::string>& out_vars)
{
  if (node.is_string()) {
    const auto& s = node.get<std::string>();
    if (!s.empty() && s.front() == '$')
      out_vars.insert(s.substr(1));
  } else if (node.is_object()) {
    for (auto& [k, v] : node.items())
      collect_config_vars(v, out_vars);
  } else if (node.is_array()) {
    for (auto& elem : node)
      collect_config_vars(elem, out_vars);
  }
}

int main(int argc, char** argv) // NOLINT(bugprone-exception-escape)
{
  CLI::App app{"A microarchitecture simulator for research and education"};

  std::string config_file_path;
  bool knob_cloudsuite{false};
  bool knob_dump{false};
  long long warmup_instructions = 0;
  long long simulation_instructions = std::numeric_limits<long long>::max();
  std::string json_file_name;
  std::vector<std::string> trace_names;

  app.add_option("--config", config_file_path, "Path to the JSON configuration file (use \"-\" for stdin)");
  app.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app.add_flag("--dump", knob_dump, "Print each module builder's parameters as modules are constructed");
  auto* warmup_instr_option = app.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  auto* deprec_warmup_instr_option =
      app.add_option("--warmup_instructions", warmup_instructions, "[deprecated] use --warmup-instructions instead")->excludes(warmup_instr_option);
  auto* sim_instr_option = app.add_option("-i,--simulation-instructions", simulation_instructions,
                                          "The number of instructions in the detailed phase. If not specified, run to the end of the trace.");
  auto* deprec_sim_instr_option =
      app.add_option("--simulation_instructions", simulation_instructions, "[deprecated] use --simulation-instructions instead")->excludes(sim_instr_option);
  auto* json_option =
      app.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")->expected(0, 1);

  // First CLI pass reads the config file path; the second pass uses the resolved core count for trace validation.
  app.allow_extras(true);
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  if (knob_dump)
    fmt::print("=== Module Builder Dump ===\n");

  // Read JSON config from file or stdin
  nlohmann::json config_json;
  if (config_file_path == "-") {
    try {
      config_json = nlohmann::json::parse(std::cin);
    } catch (const nlohmann::json::parse_error& e) {
      fmt::print("ERROR: Failed to parse JSON from stdin: {}\n", e.what());
      return 1;
    }
  } else {
    if (config_file_path.empty())
      config_file_path = "champsim_config.json";
    std::ifstream config_stream(config_file_path);
    if (config_stream.is_open()) {
      try {
        config_json = nlohmann::json::parse(config_stream);
      } catch (const nlohmann::json::parse_error& e) {
        fmt::print("ERROR: Failed to parse JSON config file {}: {}\n", config_file_path, e.what());
        return 1;
      }
    }
  }

  if (config_json.contains("_description") && config_json["_description"].is_string()) {
    fmt::print(stderr, "\nConfig: {}\n\n", config_json["_description"].get<std::string>());
  }

  // Parse config for system parameters
  std::string env_model = config_json.value("environment", std::string("LEGACY_ENVIRONMENT"));
  bool is_legacy_env = (env_model == "LEGACY_ENVIRONMENT");
  // CLI expects one trace per packet producer. The legacy env spawns one producer per core,
  // so its producer count == num_cores; explicit envs declare producers in the config and accept any count.
  std::size_t legacy_num_producers = config_json.value("num_cores", 1u);

  // "cycle_skip" (default true) lets idle operables skip cycles; false forces operate() each cycle (A/B verification switch).
  champsim::operable::set_skip_enabled(config_json.value("cycle_skip", true));

  // Scan config for $varname refs not covered by explicit CLI options; each becomes a
  // --varname option in the second pass, substituted into module params via cli_args.
  static const std::set<std::string> builtin_cli_vars = {"warmup_instructions", "simulation_instructions", "cloudsuite"};
  std::set<std::string> raw_config_vars;
  collect_config_vars(config_json, raw_config_vars);
  std::map<std::string, std::string> dynamic_cli_vars;
  for (const auto& vn : raw_config_vars) {
    if (builtin_cli_vars.count(vn))
      continue;
    // $traceN vars are handled via the positional traces argument
    if (vn.size() > 5 && vn.substr(0, 5) == "trace" && std::all_of(vn.begin() + 5, vn.end(), ::isdigit))
      continue;
    dynamic_cli_vars[vn] = "";
  }

  // Second CLI parse with full validation
  bool hide_heartbeat = false;
  CLI::App app2{"A microarchitecture simulator for research and education"};
  app2.add_option("--config", config_file_path, "Path to the JSON configuration file");
  app2.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app2.add_flag("--dump", knob_dump, "Print each module builder's parameters as modules are constructed");
  app2.add_flag("--hide-heartbeat", hide_heartbeat, "Hide the heartbeat output");
  warmup_instr_option = app2.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  deprec_warmup_instr_option =
      app2.add_option("--warmup_instructions", warmup_instructions, "[deprecated] use --warmup-instructions instead")->excludes(warmup_instr_option);
  sim_instr_option = app2.add_option("-i,--simulation-instructions", simulation_instructions,
                                     "The number of instructions in the detailed phase. If not specified, run to the end of the trace.");
  deprec_sim_instr_option =
      app2.add_option("--simulation_instructions", simulation_instructions, "[deprecated] use --simulation-instructions instead")->excludes(sim_instr_option);
  for (auto& [vn, val] : dynamic_cli_vars)
    app2.add_option("--" + vn, val, "Config variable: $" + vn);
  json_option =
      app2.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")->expected(0, 1);

  // Legacy env requires exactly legacy_num_producers traces; explicit envs allow any (resolved via $traceN).
  auto* trace_option = app2.add_option("traces", trace_names, "The paths to the traces");
  if (is_legacy_env) {
    trace_option->required()->expected(static_cast<int>(legacy_num_producers))->check(CLI::ExistingFile);
  } else {
    trace_option->check(CLI::ExistingFile);
  }

  CLI11_PARSE(app2, argc, argv);

  const bool warmup_given = (warmup_instr_option->count() > 0) || (deprec_warmup_instr_option->count() > 0);
  const bool simulation_given = (sim_instr_option->count() > 0) || (deprec_sim_instr_option->count() > 0);

  if (deprec_warmup_instr_option->count() > 0) {
    fmt::print("WARNING: option --warmup_instructions is deprecated. Use --warmup-instructions instead.\n");
  }

  if (deprec_sim_instr_option->count() > 0) {
    fmt::print("WARNING: option --simulation_instructions is deprecated. Use --simulation-instructions instead.\n");
  }

  if (simulation_given && !warmup_given) {
    // Warmup is 20% by default
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    warmup_instructions = simulation_instructions / 5;
  }

  // Construct the environment (after all CLI args are known); build a CLI args map for $-variable substitution.
  nlohmann::json cli_args = nlohmann::json::object();
  cli_args["warmup_instructions"] = warmup_instructions;
  cli_args["simulation_instructions"] = simulation_instructions;
  cli_args["cloudsuite"] = knob_cloudsuite;
  // --hide-heartbeat is an input to whatever builds the heartbeat, so it travels with the config.
  if (hide_heartbeat) {
    config_json["hide_heartbeat"] = true;
  }
  // Populate dynamic $-variables collected from the config; coerce to numeric where possible
  for (auto& [vn, val] : dynamic_cli_vars) {
    try {
      cli_args[vn] = std::stoll(val);
      continue;
    } catch (...) {
    }
    try {
      cli_args[vn] = std::stod(val);
      continue;
    } catch (...) {
    }
    cli_args[vn] = val;
  }
  for (std::size_t i = 0; i < trace_names.size(); ++i) {
    cli_args[fmt::format("trace{}", i)] = trace_names[i];
  }

  auto env_builder = champsim::modules::ModuleBuilder("environment", env_model)
                         .add_parameter("config_json", config_json)
                         .add_parameter("traces", trace_names)
                         .add_parameter("cloudsuite", knob_cloudsuite)
                         .add_parameter("repeat", simulation_given)
                         .add_parameter("cli_args", cli_args);
  champsim::modules::ModuleBuilder::set_dump_enabled(knob_dump);
  auto* gen_environment = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));

  if (knob_dump)
    fmt::print("=== End Module Builder Dump ===\n");

  fmt::print("\n*** ChampSim Multicore Out-of-Order Simulator ***\n");
  // Each phase controller reports its own plan; main does not need the phase list.
  for (champsim::modules::phase_controller& pc : gen_environment->typed_view<champsim::modules::phase_controller>("phase_controller"))
    pc.print_phase_plan();
  // Module counts: every registered interface with instances is listed, labelled by the plural
  // it reports at registration (falling back to its interface name), so nothing here is
  // hardcoded and a new interface appears automatically.
  for (const auto& iface : champsim::modules::interface_registry::get_interface_names()) {
    const auto count = gen_environment->get_num(iface);
    if (count == 0) {
      continue;
    }
    const auto label = champsim::modules::interface_registry::interface_display_name(iface);
    fmt::print("{}: {}\n", label.empty() ? iface : label, count);
  }
  fmt::print("Page size: {}\n\n", gen_environment->get_page_size());

  auto phase_stats = champsim::main(*gen_environment);

  fmt::print("\nChampSim completed all phases\n\n");

  champsim::plain_printer{std::cout}.print(phase_stats);

  for (champsim::operable& op : gen_environment->typed_view<champsim::operable>("operable")) {
    op.end_simulation();
  }

  if (json_option->count() > 0) {
    if (json_file_name.empty()) {
      champsim::json_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream json_file{json_file_name};
      champsim::json_printer{json_file}.print(phase_stats);
    }
  }

  return 0;
}
