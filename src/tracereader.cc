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

#include "tracereader.h"

#include <fstream>
#include <string>

#include "repeatable.h"
#include "inf_stream.h"

namespace champsim
{
uint64_t tracereader::instr_unique_id = 0;

ooo_model_instr apply_branch_target(ooo_model_instr branch, const ooo_model_instr& target)
{
  branch.branch_target = (branch.is_branch && branch.branch_taken) ? target.ip : 0;
  return branch;
}

template <typename T, typename S>
using reader_t = champsim::repeatable<champsim::bulk_tracereader<T, S>, uint8_t, std::string>;

template <typename T>
champsim::tracereader get_tracereader_for_type(std::string fname, uint8_t cpu)
{
  bool is_gzip_compressed = (fname.substr(std::size(fname)-2) == "gz");
  bool is_lzma_compressed = (fname.substr(std::size(fname)-2) == "xz");

  if (is_gzip_compressed)
    return champsim::tracereader{reader_t<T, champsim::inf_istream<champsim::decomp_tags::gzip_tag_t<>>>(cpu, fname)};
  else if (is_lzma_compressed)
    return champsim::tracereader{reader_t<T, champsim::inf_istream<champsim::decomp_tags::lzma_tag_t<>>>(cpu, fname)};
  else
    return champsim::tracereader{reader_t<T, std::ifstream>(cpu, fname)};
}
} // namespace champsim

champsim::tracereader get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
  if (is_cloudsuite)
    return champsim::get_tracereader_for_type<cloudsuite_instr>(fname, cpu);
  else
    return champsim::get_tracereader_for_type<input_instr>(fname, cpu);
}
