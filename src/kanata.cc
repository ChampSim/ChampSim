/*
 *    Copyright 2025 The ChampSim Contributors
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

#include "kanata.h"

#include "instruction.h"

champsim::Kanata::Instr::~Instr()
{
  kanata.print("R\t{}\t{}\t0\n", id, id);
  ++kanata.num_retired_instrs;
}

void champsim::Kanata::Lane::start(const std::string& stage) const
{
  if (instr)
    instr->kanata.print("S\t{}\t{}\t{}\n", instr->id, id, stage);
}

void champsim::Kanata::Lane::depend(uint64_t producer) const
{
  if (instr)
    instr->kanata.print("W\t{}\t{}\t0\n", instr->id, producer);
}

void champsim::Kanata::open(const std::string& path, uint64_t skip, uint64_t max)
{
  stream.exceptions(stream.badbit | stream.failbit | stream.eofbit);

  if (!path.empty())
    stream.open(path);
  else if (stream.is_open())
    stream.close();

  current_cycle = 0;
  first = skip;

  if (skip < UINT64_MAX - max) {
    last = skip + max;
    if (last < UINT64_MAX)
      ++last;
  } else {
    last = UINT64_MAX;
  }

  num_initiated_instrs = 0;
  num_retired_instrs = 0;
  initiating = false;
}

void champsim::Kanata::cycle()
{
  ++current_cycle;

  if (initiating || num_retired_instrs < num_initiated_instrs)
    print("C\t1\n");
}

void champsim::Kanata::initiate(uint32_t cpu, ooo_model_instr& instr)
{
  if (instr.instr_id < first || instr.instr_id > last)
    return;

  if (!initiating) {
    print("Kanata\t0004\nC=\t{}\n", current_cycle);
    initiating = true;
  }

  instr.kanata = initiate(cpu, instr.instr_id, fmt::streamed(instr));

  if (instr.instr_id >= last)
    initiating = false;
}
