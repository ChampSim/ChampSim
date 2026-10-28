/*
 * Built-in JSON type-key converters (e.g. {"frequency": "4G"} -> typed std::any).
 * Modules add more via static champsim::type_registry::register_type_helper objects.
 */

#include "type_registry.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <fmt/core.h>

#include "access_type.h"
#include "bandwidth.h"
#include "chrono.h"
#include "modules.h"
#include "util/units.h"

using json = nlohmann::json;

namespace
{

// ====== Shared parsing helpers ======

std::pair<double, std::string> parse_number_and_suffix(const std::string& s)
{
  std::size_t pos = 0;
  double value = std::stod(s, &pos);
  return {value, s.substr(pos)};
}

// Look up a suffix multiplier (empty suffix → 1); error-exit on an unknown suffix.
long long lookup_multiplier(const std::string& suffix, std::initializer_list<std::pair<const char*, long long>> table, const std::string& s)
{
  if (suffix.empty())
    return 1LL;
  for (const auto& [suf, mult] : table)
    if (suffix == suf)
      return mult;
  fmt::print("[TYPE_REGISTRY] ERROR: unknown suffix '{}' in '{}'\n", suffix, s);
  std::exit(-1);
}

// ====== Individual type parsers ======

// "frequency": "4G"  →  champsim::chrono::picoseconds (period)
std::any parse_frequency(const json& val)
{
  auto s = val.get<std::string>();
  auto [value, suffix] = parse_number_and_suffix(s);
  auto multiplier = static_cast<double>(lookup_multiplier(suffix, {{"K", 1000LL}, {"M", 1000000LL}, {"G", 1000000000LL}, {"T", 1000000000000LL}}, s));
  auto ps = static_cast<int64_t>(std::round(1e12 / (value * multiplier)));
  return champsim::chrono::picoseconds{ps};
}

// "time": "200n"  →  champsim::chrono::{pico,nano,micro,milli,}seconds
std::any parse_time(const json& val)
{
  auto s = val.get<std::string>();
  auto [value, suffix] = parse_number_and_suffix(s);
  auto int_val = static_cast<int64_t>(std::round(value));
  if (suffix.empty() || suffix == "p")
    return champsim::chrono::picoseconds{int_val};
  else if (suffix == "n")
    return champsim::chrono::nanoseconds{int_val};
  else if (suffix == "u")
    return champsim::chrono::microseconds{int_val};
  else if (suffix == "m")
    return champsim::chrono::milliseconds{int_val};
  else if (suffix == "s")
    return champsim::chrono::seconds{int_val};
  fmt::print("[TYPE_REGISTRY] ERROR: unknown time suffix '{}' in '{}'\n", suffix, s);
  std::exit(-1);
}

// "bytes": "2Mi"  →  champsim::data::bytes (always stored as bytes base unit)
std::any parse_bytes(const json& val)
{
  auto s = val.get<std::string>();
  auto [value, suffix] = parse_number_and_suffix(s);
  auto int_val = static_cast<long long>(std::round(value));
  auto mult = lookup_multiplier(suffix,
                                {{"Ki", 1024LL},
                                 {"Mi", 1048576LL},
                                 {"Gi", 1073741824LL},
                                 {"Ti", 1099511627776LL},
                                 {"K", 1000LL},
                                 {"M", 1000000LL},
                                 {"G", 1000000000LL},
                                 {"T", 1000000000000LL}},
                                s);
  return champsim::data::bytes{int_val * mult};
}

// "bits": "64"  →  champsim::data::bits
std::any parse_bits(const json& val)
{
  auto s = val.get<std::string>();
  auto [value, suffix] = parse_number_and_suffix(s);
  auto int_val = static_cast<unsigned long long>(std::round(value));
  auto mult = static_cast<unsigned long long>(lookup_multiplier(suffix, {{"K", 1000LL}, {"M", 1000000LL}, {"G", 1000000000LL}}, s));
  return champsim::data::bits{int_val * mult};
}

// "bandwidth": 2  →  champsim::bandwidth::maximum_type
std::any parse_bandwidth(const json& val) { return champsim::bandwidth::maximum_type{val.get<long long>()}; }

// "optional_uint64": 42 | null  →  std::optional<uint64_t>
std::any parse_optional_uint64(const json& val)
{
  if (val.is_null())
    return std::optional<uint64_t>{};
  return std::optional<uint64_t>{val.get<uint64_t>()};
}

// "null": "channel"  →  typed null pointer via interface_registry
std::any parse_null_pointer(const json& val) { return champsim::modules::interface_registry::make_null_pointer(val.get<std::string>()); }

// "access_types": ["LOAD", "PREFETCH"]  →  std::vector<access_type>
std::any parse_access_types(const json& val)
{
  std::vector<access_type> result;
  for (auto& elem : val) {
    auto at = access_type_from_string(elem.get<std::string>());
    if (at != access_type::NUM_TYPES)
      result.push_back(at);
  }
  return result;
}

} // anonymous namespace

// ====== Static registrations ======
// These run before main(), guaranteeing availability when any environment parses its config.

static champsim::type_registry::register_type_helper reg_frequency("frequency", parse_frequency);
static champsim::type_registry::register_type_helper reg_time("time", parse_time);
static champsim::type_registry::register_type_helper reg_bytes("bytes", parse_bytes);
static champsim::type_registry::register_type_helper reg_bits("bits", parse_bits);
static champsim::type_registry::register_type_helper reg_bandwidth("bandwidth", parse_bandwidth);
static champsim::type_registry::register_type_helper reg_optional_uint64("optional_uint64", parse_optional_uint64);
static champsim::type_registry::register_type_helper reg_null("null", parse_null_pointer);
static champsim::type_registry::register_type_helper reg_access_types("access_types", parse_access_types);

// ====== Default kind converters ======
// Cover plain scalars/arrays; modules can override any via register_kind.

namespace
{
// "true" / "false"  →  bool
std::any kind_bool(const json& v) { return v.get<bool>(); }
// 42                →  int64_t (signed)
std::any kind_integer(const json& v) { return v.get<int64_t>(); }
// 42                →  uint64_t (unsigned-tagged)
std::any kind_unsigned(const json& v) { return v.get<uint64_t>(); }
// 1.5               →  double
std::any kind_float(const json& v) { return v.get<double>(); }
// "foo"             →  std::string
std::any kind_string(const json& v) { return v.get<std::string>(); }
// {anything}        →  raw nlohmann::json (caller may further process)
std::any kind_object_passthrough(const json& v) { return v; }

// Arrays: try common shapes, else fall back to raw json for the caller.
std::any kind_array(const json& v)
{
  if (!v.empty() && v[0].is_string()) {
    std::vector<std::string> sv;
    for (auto& e : v)
      sv.push_back(e.get<std::string>());
    return sv;
  }
  if (!v.empty() && v[0].is_array()) {
    std::vector<std::array<uint32_t, 3>> dims;
    for (std::size_t i = 0; i < v.size(); i++) {
      std::array<uint32_t, 3> entry{};
      for (std::size_t j = 0; j < v[i].size() && j < 3; j++)
        entry[j] = static_cast<uint32_t>(v[i][j].get<int64_t>());
      dims.push_back(entry);
    }
    return dims;
  }
  // Pass-through for anything else
  return v;
}
} // namespace

static champsim::type_registry::register_kind_helper reg_bool(champsim::type_registry::kind::boolean, kind_bool);
static champsim::type_registry::register_kind_helper reg_int(champsim::type_registry::kind::integer, kind_integer);
static champsim::type_registry::register_kind_helper reg_uint(champsim::type_registry::kind::unsigned_integer, kind_unsigned);
static champsim::type_registry::register_kind_helper reg_float(champsim::type_registry::kind::floating_point, kind_float);
static champsim::type_registry::register_kind_helper reg_string(champsim::type_registry::kind::string, kind_string);
static champsim::type_registry::register_kind_helper reg_array(champsim::type_registry::kind::array, kind_array);
static champsim::type_registry::register_kind_helper reg_object(champsim::type_registry::kind::object, kind_object_passthrough);
