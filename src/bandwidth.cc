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

#include "bandwidth.h"

#include <stdexcept>
#include <string>

#include "util/to_underlying.h"

champsim::bandwidth::bandwidth(maximum_type maximum) : value_(champsim::to_underlying(maximum)), maximum_(maximum) {}

void champsim::bandwidth::consume(underlying_type delta)
{
  value_ -= delta;
  if (value_ < 0) {
    throw std::range_error{"Exceeded bandwidth of " + std::to_string(champsim::to_underlying(maximum_))};
  }
}

void champsim::bandwidth::consume() { consume(1); }

bool champsim::bandwidth::has_remaining() const { return amount_remaining() > 0; }

auto champsim::bandwidth::amount_consumed() const -> underlying_type { return champsim::to_underlying(maximum_) - value_; }

auto champsim::bandwidth::amount_remaining() const -> underlying_type { return value_; }

void champsim::bandwidth::reset() { value_ = champsim::to_underlying(maximum_); }
