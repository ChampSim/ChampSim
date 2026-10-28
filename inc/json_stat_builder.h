/*
 *    Copyright 2026 The ChampSim Contributors
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

#ifndef JSON_STAT_BUILDER_H
#define JSON_STAT_BUILDER_H

#include <string>
#include <utility>
#include <nlohmann/json.hpp>

namespace champsim
{

// Fluent helper for building a module's JSON stats via add()/group()/push_back();
// the framework wraps each dump under [interface][model][name].
class json_stat_builder
{
public:
  /** Construct an empty builder backed by its own JSON object. */
  json_stat_builder() : own_(nlohmann::json::object()), node_(&own_) {}

  /** Construct a builder that writes into an external JSON node (used for groups). */
  explicit json_stat_builder(nlohmann::json& target) : node_(&target)
  {
    if (!node_->is_object())
      *node_ = nlohmann::json::object();
  }

  // Add a named stat (any nlohmann::json-compatible type); returns *this for chaining.
  template <typename T>
  json_stat_builder& add(const std::string& name, T&& value)
  {
    (*node_)[name] = std::forward<T>(value);
    return *this;
  }

  // Begin a nested object under name; returned builder borrows the parent node.
  json_stat_builder group(const std::string& name)
  {
    auto& sub = (*node_)[name];
    if (!sub.is_object())
      sub = nlohmann::json::object();
    return json_stat_builder{sub};
  }

  /** Append a value to a named array under the current object. */
  template <typename T>
  json_stat_builder& push_back(const std::string& name, T&& value)
  {
    auto& arr = (*node_)[name];
    if (!arr.is_array())
      arr = nlohmann::json::array();
    arr.push_back(std::forward<T>(value));
    return *this;
  }

  /** Direct access to the underlying JSON node (read-only). */
  const nlohmann::json& json() const { return *node_; }
  /** Direct access to the underlying JSON node (mutable, escape hatch). */
  nlohmann::json& json() { return *node_; }

  /** True if no stats have been added. */
  bool empty() const { return node_->empty(); }

private:
  nlohmann::json own_;   // backing storage when not borrowing
  nlohmann::json* node_; // points to own_ or to an external node
};

} // namespace champsim

#endif // JSON_STAT_BUILDER_H
