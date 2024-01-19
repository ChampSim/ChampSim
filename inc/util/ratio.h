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

#ifndef UTIL_RATIO_H
#define UTIL_RATIO_H

#include <ratio>

namespace champsim
{
using kibi = std::ratio<(1LL << 10)>;
using mebi = std::ratio<(1LL << 20)>;
using gibi = std::ratio<(1LL << 30)>;
using tebi = std::ratio<(1LL << 40)>;
using pebi = std::ratio<(1LL << 50)>;
using exbi = std::ratio<(1LL << 60)>;
} // namespace champsim

#endif
