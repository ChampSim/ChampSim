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

/*
#ifdef CHAMPSIM_MODULE
#error "Modules should include msl/bits.h"
#endif
*/

#ifndef UTIL_BITS_H
#define UTIL_BITS_H

#include <utility>

#include "../msl/bits.h"

namespace champsim
{
using msl::bitmask;
using msl::ipow;
using msl::is_power_of_2;
using msl::lg2;
using msl::next_pow2;
using msl::splice_bits;
} // namespace champsim

#endif
