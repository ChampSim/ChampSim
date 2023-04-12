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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <string>

#include "repeatable.h"

namespace champsim
{
namespace detail
{
struct pcloser {
  void operator()(FILE* f) const { pclose(f); }
};
} // namespace detail

std::string get_fptr_cmd(std::string_view fname)
{
  using namespace std::literals::string_literals;
  std::string unzip_command{"cat "};
  if (fname.substr(0, 4) == "http")
    unzip_command = "curl -s "s;
  unzip_command += fname.data();

  auto last_dot = fname.substr(fname.find_last_of("."));
  if (last_dot == ".gz") // gzip format
    unzip_command += " | gzip -dc";
  else if (last_dot == ".xz") // xz
    unzip_command += " | xz -dc";

  return unzip_command;
}

struct popen_istream {
  std::unique_ptr<FILE, detail::pcloser> fp{nullptr};
  std::streamsize gcount_;
  bool eof_ = false;

  popen_istream& read(char* s, std::streamsize count)
  {
    gcount_ = fread(s, sizeof(char), count, fp.get());
    eof_ = !(gcount_ > 0);
    return *this;
  }

  bool eof() const { return eof_; }
  std::streamsize gcount() const { return gcount_; }

  explicit popen_istream(std::string s) : fp(popen(s.c_str(), "r")) {}
};

uint64_t tracereader::instr_unique_id = 0;

ooo_model_instr apply_branch_target(ooo_model_instr branch, const ooo_model_instr& target)
{
  branch.branch_target = (branch.is_branch && branch.branch_taken) ? target.ip : 0;
  return branch;
}
} // namespace champsim

template <typename T, typename S>
using reader_t = champsim::repeatable<champsim::bulk_tracereader<T, S>, uint8_t, std::string>;
champsim::tracereader get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
  bool is_url = (fname.substr(0, 4) == "http");
  bool is_compressed = (fname.substr(std::size(fname)-2) == "gz") || (fname.substr(std::size(fname)-2) == "xz");

  if (is_url || is_compressed) {
    auto fptr_cmd = champsim::get_fptr_cmd(fname);
    if (is_cloudsuite)
      return champsim::tracereader{reader_t<cloudsuite_instr, champsim::popen_istream>(cpu, fptr_cmd)};
    else
      return champsim::tracereader{reader_t<input_instr, champsim::popen_istream>(cpu, fptr_cmd)};
  } else {
    if (is_cloudsuite)
      return champsim::tracereader{reader_t<cloudsuite_instr, std::ifstream>(cpu, fname)};
    else
      return champsim::tracereader{reader_t<input_instr, std::ifstream>(cpu, fname)};
  }
}
