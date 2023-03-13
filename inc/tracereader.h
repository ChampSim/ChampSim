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

#include <iostream>
#include <memory>
#include <string>

#include "instruction.h"

namespace champsim
{
  class tracereader
  {
    struct reader_concept
    {
      static uint64_t instr_unique_id;
      virtual ~reader_concept() = default;
      virtual ooo_model_instr operator()() = 0;
      virtual std::string trace_string() const = 0;
    };

    template <typename T>
    struct reader_model final : public reader_concept
    {
      T intern_;
      reader_model(T&& val) : intern_(std::move(val)) {}

      ooo_model_instr operator()() override {
        auto retval = intern_();
        retval.instr_id = instr_unique_id++;

        // Reopen trace if we've reached the end of the file
        if (intern_.eof()) {
          auto name = intern_.trace_string;
          std::cout << "*** Reached end of trace: " << name << std::endl;
          intern_.restart();
        }

        return retval;
      }

      std::string trace_string() const { return intern_.trace_string; }
    };

    std::unique_ptr<reader_concept> pimpl_;

    public:

    template <typename T>
    tracereader(T&& val) : pimpl_(std::make_unique<reader_model<T>>(std::move(val))) {}

    auto operator()() { return (*pimpl_)(); }
    std::string trace_string() const { return pimpl_->trace_string(); }
  };
}

champsim::tracereader get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

#endif
