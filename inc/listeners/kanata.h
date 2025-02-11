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

#include <deque>
#include <functional>
#include <optional>
#include <CLI/CLI.hpp>
#include <fmt/os.h>
#include <fmt/ostream.h>

#include "cache.h"
#include "dram_controller.h"
#include "events.h"
#include "instruction.h"
#include "ooo_cpu.h"
#include "ptw.h"

namespace champsim
{
namespace kanata
{
class Kanata final
{
public:
  static constexpr auto cli_key = "Kanata";

  void cli(CLI::App& app)
  {
    app.add_option("--kanata", file_name, "The name of the file to receive Kanata output");
    app.add_option("--kanata-max", max, "The maximum number of instructions for Kanata logging");
    app.add_option("--kanata-skip", skip, "The number of instructions to skip before starting Kanata logging");
  }

  template <Event e, typename... Args>
  inline void handle_event(Args&&... args);

private:
  struct Instr final {
    Kanata& kanata;
    const uint64_t id;
    const bool flushed;

    Instr(Kanata& kanata_, uint64_t id_, bool flushed_) : kanata(kanata_), id(id_), flushed(flushed_) {}

    ~Instr();
    void start(uint8_t lane, const std::string& stage);
  };

  template <typename T>
  class Queue final
  {
  public:
    T& emplace(uint64_t index)
    {
      if (storage.empty())
        begin = index;
      else
        assert(index - begin >= storage.size());

      storage.resize(index - begin + 1);

      return storage[index - begin];
    }

    T* find(uint64_t index) { return index >= begin && index - begin < storage.size() ? &storage[index - begin] : nullptr; }

    T& front() { return storage.front(); }

    bool empty() const { return storage.empty(); }

    void pop()
    {
      storage.pop_front();
      ++begin;
    }

  private:
    uint64_t begin = UINT64_MAX;
    std::deque<T> storage;
  };

  template <typename ID, typename Label>
  std::shared_ptr<Instr> init(uint32_t cpu, ID id, Label label, bool flushed)
  {
    auto index = num;

    ++num;

    if (index < skip || index - skip > max)
      return {};

    if (!file) {
      if (!file_name) {
        skip = UINT64_MAX;
        return {};
      }

      file.emplace(fmt::output_file(*file_name));
      file->print("Kanata\t0004\nC=\t{}\n", current_cycle);
    }

    auto instr = std::make_shared<Instr>(*this, num - skip, flushed);
    file->print("I\t{}\t{}\t{}\nL\t{}\t0\t{}\n", instr->id, id, cpu, instr->id, label);
    return instr;
  }

  void init(const O3_CPU& cpu, const ooo_model_instr& o3)
  {
    auto instr = init(cpu.cpu, o3.instr_id, fmt::streamed(o3), false);
    if (!instr)
      return;

    if (cpu.cpu >= cpus.size())
      cpus.resize(cpu.cpu + 1);

    auto& emplaced = cpus[cpu.cpu].emplace(o3.instr_id);
    emplaced.swap(instr);
    emplaced->start(0, "F");
  }

  void retire();

  void start(const O3_CPU& cpu, const ooo_model_instr& o3, const std::string& stage)
  {
    if (cpu.cpu >= cpus.size())
      return;

    auto kanata = cpus[cpu.cpu].find(o3.instr_id);
    if (!kanata)
      return;

    (*kanata)->start(0, stage);
  }

  std::optional<fmt::ostream> file;
  std::optional<std::string> file_name;
  std::vector<Queue<std::shared_ptr<Instr>>> cpus;
  uint64_t current_cycle = 0;
  uint64_t max = UINT64_MAX;
  uint64_t num = 0;
  uint64_t skip = 0;

  template <Event e, typename... Args>
  friend void handle_event(Kanata& kanata, const Args&... args);
};

template <Event e, typename... Args>
inline void handle_event([[maybe_unused]] Kanata& kanata, [[maybe_unused]] const Args&... args)
{
}

template <>
inline void handle_event<Event::COMPLETE>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "C");
}

template <>
inline void handle_event<Event::CYCLE>(Kanata& kanata)
{
  ++kanata.current_cycle;

  if (kanata.file)
    kanata.file->print("C\t1\n");
}

template <>
inline void handle_event<Event::DECODE>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "Dc");
}

template <>
inline void handle_event<Event::DIB_HIT>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "Dh");
}

template <>
inline void handle_event<Event::DISPATCH>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "Ds");
}

template <>
inline void handle_event<Event::EXEC>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "X");
}

template <>
inline void handle_event<Event::INIT>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.init(cpu, instr);
}

template <>
inline void handle_event<Event::ISSUE>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  if (cpu.cpu >= kanata.cpus.size())
    return;

  auto kanata_instr = kanata.cpus[cpu.cpu].find(instr.instr_id);
  if (!kanata_instr)
    return;

  for (auto& source : instr.source_registers) {
    auto producer = kanata.cpus[cpu.cpu].find(cpu.reg_allocator.get_physical_register(source).producing_instruction_id);
    if (!producer)
      continue;

    kanata.file->print("W\t{}\t{}\t0\n", (*kanata_instr)->id, (*producer)->id);
  }

  (*kanata_instr)->start(0, "I");
}

template <>
inline void handle_event<Event::RENAME>(Kanata& kanata, const O3_CPU& cpu, const ooo_model_instr& instr)
{
  kanata.start(cpu, instr, "R");
}

template <>
inline void handle_event<Event::RETIRE>(Kanata& kanata, [[maybe_unused]] const uint32_t& cpu, const std::deque<ooo_model_instr>::const_iterator& begin,
                                        const std::deque<ooo_model_instr>::const_iterator& end, [[maybe_unused]] const uint64_t& current_cycles)
{
  if (cpu >= kanata.cpus.size())
    return;

  for (auto it = begin; it != end; ++it) {
    auto instr = kanata.cpus[cpu].find(it->instr_id);
    if (!instr)
      continue;

    (*instr)->start(0, "S");
    instr->reset();
  }

  while (!kanata.cpus[cpu].empty() && !kanata.cpus[cpu].front())
    kanata.cpus[cpu].pop();

  kanata.retire();
}

template <Event e, typename... Args>
inline void Kanata::handle_event(Args&&... args)
{
  kanata::handle_event<e>(*this, std::forward<Args>(args)...);
}
} // namespace kanata
} // namespace champsim

#endif
