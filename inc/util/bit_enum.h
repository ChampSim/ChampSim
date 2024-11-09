/*
 *    Copyright 2024 The ChampSim Contributors
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

#ifndef UTIL_BIT_ENUM_H
#define UTIL_BIT_ENUM_H

#include <cstdint>
#include <limits>

#include "to_underlying.h"

namespace champsim
{
namespace data
{

/**
 * A strong type to represent a bit width. Being an enum prevents arbitrary arithmetic from being performed.
 */
enum class bits : uint64_t {};
} // namespace data
} // namespace champsim

constexpr champsim::data::bits operator+(champsim::data::bits lhs, champsim::data::bits rhs)
{
  return champsim::data::bits{champsim::to_underlying(lhs) + champsim::to_underlying(rhs)};
}

constexpr champsim::data::bits operator*(unsigned long long lhs, champsim::data::bits rhs) { return champsim::data::bits{lhs * champsim::to_underlying(rhs)}; }
constexpr champsim::data::bits operator*(champsim::data::bits lhs, unsigned long long rhs) { return rhs * lhs; }

constexpr auto operator/(champsim::data::bits lhs, champsim::data::bits rhs) { return champsim::to_underlying(lhs) / champsim::to_underlying(rhs); }
constexpr champsim::data::bits operator%(champsim::data::bits lhs, champsim::data::bits rhs)
{
  return champsim::data::bits{champsim::to_underlying(lhs) % champsim::to_underlying(rhs)};
}

#endif
