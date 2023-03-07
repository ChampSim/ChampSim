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
#include <cstring>
#include <string>

#if defined(__GNUG__) && !defined(__APPLE__)
#include <iostream>
#endif

uint64_t tracereader::instr_unique_id = 0;
void detail::pclose_file(FILE* f) { pclose(f); }

FILE* tracereader::get_fptr(std::string fname)
{
  std::string cmd_fmtstr = "%1$s %2$s";
  if (fname.substr(0, 4) == "http")
    cmd_fmtstr = "wget -qO- -o /dev/null %2$s | %1$s";

  std::string decomp_program = "cat";
  if (fname.back() == 'z') {
    std::string last_dot = fname.substr(fname.find_last_of("."));
    if (last_dot[1] == 'g') // gzip format
      decomp_program = "gzip -dc";
    else if (last_dot[1] == 'x') // xz
      decomp_program = "xz -dc";
  }

  char gunzip_command[4096];
  snprintf(gunzip_command, std::size(gunzip_command), cmd_fmtstr.c_str(), decomp_program.c_str(), fname.c_str());
  return popen(gunzip_command, "r");
}

template <typename T>
void tracereader::refresh_buffer()
{
  std::array<T, buffer_size - refresh_thresh> trace_read_buf;
  std::array<char, std::size(trace_read_buf) * sizeof(T)> raw_buf;
  std::size_t bytes_read;

  // Read from trace file
#if defined(__GNUG__) && !defined(__APPLE__)
  std::istream trace_file{&filebuf};
  trace_file.read(std::data(raw_buf), std::size(raw_buf));
  bytes_read = static_cast<std::size_t>(trace_file.gcount());
  eof_ = trace_file.eof();
#else
  bytes_read = fread(std::data(raw_buf), sizeof(char), std::size(raw_buf), fp);
  eof_ = !(bytes_read > 0);
#endif

  // Transform bytes into trace format instructions
  std::memcpy(std::data(trace_read_buf), std::data(raw_buf), bytes_read);

  // Inflate trace format into core model instructions
  auto begin = std::begin(trace_read_buf);
  auto end = std::next(begin, bytes_read / sizeof(T));
  std::transform(begin, end, std::back_inserter(instr_buffer), [cpu = this->cpu](T t) { return ooo_model_instr{cpu, t}; });

  // Set branch targets
  for (auto it = std::next(std::begin(instr_buffer)); it != std::end(instr_buffer); ++it)
    std::prev(it)->branch_target = (std::prev(it)->is_branch && std::prev(it)->branch_taken) ? it->ip : 0;
}

template <typename T>
ooo_model_instr tracereader::impl_get()
{
  if (std::size(instr_buffer) <= refresh_thresh)
    refresh_buffer<T>();

  auto retval = instr_buffer.front();
  instr_buffer.pop_front();

  retval.instr_id = instr_unique_id++;
  return retval;
}

template <typename T>
class bulk_tracereader : public tracereader
{
public:
  using tracereader::tracereader;
  ooo_model_instr operator()() override final { return impl_get<T>(); }
};

std::unique_ptr<tracereader> get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite)
{
  if (is_cloudsuite)
    return std::make_unique<bulk_tracereader<cloudsuite_instr>>(cpu, fname);
  else
    return std::make_unique<bulk_tracereader<input_instr>>(cpu, fname);
}

bool tracereader::eof() const { return eof_ && std::size(instr_buffer) <= refresh_thresh; }
