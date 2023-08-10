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

#ifndef UTIL_TO_UNDERLYING_H
#define UTIL_TO_UNDERLYING_H

namespace champsim
{
/*
 * A forward-port of C++23's function of the same name.
 * This avoids static_cast'ing an enumeration to an integer type other than its underlying type,
 * an action that could dodge -Wconversion
 */
template <typename E>
constexpr std::underlying_type_t<E> to_underlying(E e) noexcept
{
  return static_cast<std::underlying_type_t<E>>(e);
}
} // namespace champsim

#endif
