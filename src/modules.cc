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

#include "modules.h"

#include "cache.h"

bool champsim::modules::prefetcher::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
{
  return intern_->prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

// LCOV_EXCL_START Exclude deprecated function
bool champsim::modules::prefetcher::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP
