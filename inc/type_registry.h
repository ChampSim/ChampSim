/*
 * Type registry for JSON → std::any conversion. Modules register converters by
 * JSON type-key ({"frequency": "4G"}) or JSON kind (boolean, integer, ...); every
 * parameter funnels through try_convert, so modules extend it without touching the
 * environment. Built-in types and kind defaults are registered in type_registry.cc.
 */

#ifndef TYPE_REGISTRY_H
#define TYPE_REGISTRY_H

#include <any>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace champsim
{

class type_registry
{
public:
  // Converter: JSON value from {"type_key": value} -> correctly-typed std::any.
  using converter_fn = std::function<std::any(const nlohmann::json&)>;

  // JSON value kinds, for try_convert's kind-dispatch path.
  enum class kind { boolean, integer, unsigned_integer, floating_point, string, array, object };

  // Register a type-key converter; duplicate keys overwrite so modules can replace builtins.
  static void register_type(const std::string& type_key, converter_fn fn) { type_registry_map()[type_key] = std::move(fn); }

  // Register a default converter for a JSON value kind.
  static void register_kind(kind k, converter_fn fn) { kind_registry_map()[k] = std::move(fn); }

  // Convert any JSON value to a typed std::any: single-key typed object -> its type-key
  // converter, else dispatch on JSON kind. Returns true on success.
  static bool try_convert(const nlohmann::json& val, std::any& out)
  {
    if (val.is_object() && val.size() == 1) {
      auto it = val.begin();
      auto reg_it = type_registry_map().find(it.key());
      if (reg_it != type_registry_map().end()) {
        out = reg_it->second(it.value());
        return true;
      }
    }
    auto k = kind_of(val);
    if (!k.has_value())
      return false;
    auto reg_it = kind_registry_map().find(*k);
    if (reg_it == kind_registry_map().end())
      return false;
    out = reg_it->second(val);
    return true;
  }

  static bool has_type(const std::string& type_key) { return type_registry_map().count(type_key) > 0; }

  // RAII helper for static registration, analogous to register_module.
  struct register_type_helper {
    register_type_helper(const std::string& type_key, converter_fn fn) { type_registry::register_type(type_key, std::move(fn)); }
  };

  // RAII helper for kind registration.
  struct register_kind_helper {
    register_kind_helper(kind k, converter_fn fn) { type_registry::register_kind(k, std::move(fn)); }
  };

private:
  static std::optional<kind> kind_of(const nlohmann::json& v)
  {
    if (v.is_null())
      return std::nullopt;
    if (v.is_boolean())
      return kind::boolean;
    if (v.is_number_unsigned())
      return kind::unsigned_integer;
    if (v.is_number_integer())
      return kind::integer;
    if (v.is_number_float())
      return kind::floating_point;
    if (v.is_string())
      return kind::string;
    if (v.is_array())
      return kind::array;
    if (v.is_object())
      return kind::object;
    return std::nullopt;
  }

  static std::map<std::string, converter_fn>& type_registry_map()
  {
    static std::map<std::string, converter_fn> r;
    return r;
  }
  static std::map<kind, converter_fn>& kind_registry_map()
  {
    static std::map<kind, converter_fn> r;
    return r;
  }
};

} // namespace champsim

#endif // TYPE_REGISTRY_H
