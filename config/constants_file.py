#    Copyright 2023 The ChampSim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

def get_constants_file(env, pmem):
    yield from (
        '#ifndef CHAMPSIM_CONSTANTS_H',
        '#define CHAMPSIM_CONSTANTS_H',

        '#ifdef CHAMPSIM_MODULE',
        '#define SET_ASIDE_CHAMPSIM_MODULE',
        '#undef CHAMPSIM_MODULE',
        '#endif',

        '#include <cstdlib>',
        '#include "util/bits.h"',
        f'inline constexpr unsigned BLOCK_SIZE = {env["block_size"]};',
        f'inline constexpr unsigned PAGE_SIZE = {env["page_size"]};',
        f'inline constexpr uint64_t STAT_PRINTING_PERIOD = {env["heartbeat_frequency"]};',
        f'inline constexpr std::size_t NUM_CPUS = {env["num_cores"]};',
        'inline constexpr auto LOG2_BLOCK_SIZE = champsim::lg2(BLOCK_SIZE);',
        'inline constexpr auto LOG2_PAGE_SIZE = champsim::lg2(PAGE_SIZE);',
        f'inline constexpr uint64_t DRAM_IO_FREQ = {pmem["io_freq"]};',
        f'inline constexpr std::size_t DRAM_CHANNELS = {pmem["channels"]};',
        f'inline constexpr std::size_t DRAM_RANKS = {pmem["ranks"]};',
        f'inline constexpr std::size_t DRAM_BANKS = {pmem["banks"]};',
        f'inline constexpr std::size_t DRAM_ROWS = {pmem["rows"]};',
        f'inline constexpr std::size_t DRAM_COLUMNS = {pmem["columns"]};',
        f'inline constexpr std::size_t DRAM_CHANNEL_WIDTH = {pmem["channel_width"]};',
        f'inline constexpr std::size_t DRAM_WQ_SIZE = {pmem["wq_size"]};',
        f'inline constexpr std::size_t DRAM_RQ_SIZE = {pmem["rq_size"]};',
        f'inline constexpr char RAMULATOR_CONFIG[] = "{pmem["ramulator_config"]}";' if pmem["model"] == "ramulator" else '',
        '#ifdef SET_ASIDE_CHAMPSIM_MODULE',
        '#undef SET_ASIDE_CHAMPSIM_MODULE',
        '#define CHAMPSIM_MODULE',
        '#endif',

        '#endif')
