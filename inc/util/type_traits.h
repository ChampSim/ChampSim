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

#ifndef UTIL_TYPE_TRAITS_H
#define UTIL_TYPE_TRAITS_H

#include <type_traits>

namespace champsim
{
template <typename T, template <typename...> typename Primary>
inline constexpr bool is_specialization_v = false;

template <template <typename...> typename Primary, typename... Args>
inline constexpr bool is_specialization_v<Primary<Args...>, Primary> = true;

template <typename T, template <typename...> typename Primary>
struct is_specialization : std::bool_constant<is_specialization_v<T, Primary>> {
};
} // namespace champsim

#endif
