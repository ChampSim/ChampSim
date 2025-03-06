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
long champsim::modules::replacement::get_set_sample_rate() const
{
  long set_sample_rate = 32; // 1 in 32
  if(intern_->NUM_SET < 1024 && intern_->NUM_SET >= 256) { // 1 in 16
      set_sample_rate = 16;
  } else if(intern_->NUM_SET >= 64) { // 1 in 8
      set_sample_rate = 8;
  } else if(intern_->NUM_SET >= 8) { // 1 in 4
      set_sample_rate = 4;
  } else {
      assert(false); // Not enough sets to sample for set dueling
  }
  return set_sample_rate;
}

long champsim::modules::replacement::get_set_sample_category(long set, long set_sample_rate) const
{
  auto mask = set_sample_rate - 1;
  auto shift = champsim::lg2(set_sample_rate);
  auto low_slice = set & mask;
  auto high_slice = (set >> shift) & mask;

  // This should return 0 when low_slice == high_slice and 1 ~ (set_sample_rate - 1) otherwise
  return (set_sample_rate + low_slice - high_slice) & mask;
}

long champsim::modules::replacement::get_set_sample_category(long set) const
{
  return get_set_sample_category(set, get_set_sample_rate());
}

long champsim::modules::replacement::get_num_sampled_sets(long set_sample_rate) const
{
  // Inaccurate if not perfectly divisible
  assert(intern_->NUM_SET % set_sample_rate == 0);
  return intern_->NUM_SET / set_sample_rate;
}

long champsim::modules::replacement::get_num_sampled_sets() const
{
  return get_num_sampled_sets(get_set_sample_rate());
}
