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

#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <cstdio>
#include <string>

#include "instruction.h"

class tracereader
{
protected:
  FILE* trace_file = NULL;
  uint8_t cpu;
  std::string cmd_fmtstr;
  std::string decomp_program;
  std::string trace_string;

public:
  tracereader(const tracereader& other) = delete;
  tracereader(uint8_t cpu, std::string _ts);
  ~tracereader();
  void open(std::string trace_string);
  void close();

  template <typename T>
  ooo_model_instr read_single_instr();

  virtual ooo_model_instr get() = 0;
};

tracereader* get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

#endif
