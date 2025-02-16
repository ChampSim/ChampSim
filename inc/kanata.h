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

#ifndef KANATA_H
#define KANATA_H

#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <fmt/ostream.h>

struct ooo_model_instr;

namespace champsim
{
class Kanata;

class Kanata final
{
public:
  class Lane;

  class Instr final
  {
  public:
    const uint64_t id;
    Instr(Kanata& kanata_, uint64_t id_) : id(id_), kanata(kanata_) {}
    Instr(const Instr&) = delete;
    ~Instr();

  private:
    Kanata& kanata;
    uint8_t num_lanes = 0;
    friend Lane;
  };

  class Lane final
  {
  public:
    std::shared_ptr<Instr> instr;
    Lane(std::shared_ptr<Instr> instr_ = {}) : instr(std::move(instr_)), id(instr ? instr->num_lanes++ : UINT8_MAX) {}
    void start(const std::string& stage) const;
    void depend(uint64_t producer) const;

  private:
    uint8_t id;
  };

  void open(const std::string& path, uint64_t skip, uint64_t max);
  void cycle();
  void initiate(uint32_t cpu, ooo_model_instr& instr);

  template <class ID, class Label>
  Lane initiate(uint32_t cpu, ID id, Label label)
  {
    if (!initiating)
      return {};

    auto instr = std::make_shared<Instr>(*this, num_initiated_instrs++);
    print("I\t{}\t{}\t{}\nL\t{}\t0\t{}\n", instr->id, id, cpu, instr->id, label);
    return instr;
  }

private:
  template <class... T>
  void print(T... args)
  {
    fmt::print(stream.is_open() ? stream : std::cout, args...);
  }

  std::ofstream stream;
  uint64_t current_cycle;
  uint64_t first = UINT64_MAX;
  uint64_t last;
  uint64_t num_initiated_instrs = 0;
  uint64_t num_retired_instrs;
  bool initiating = false;
};
} // namespace champsim

#endif
