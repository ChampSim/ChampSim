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
        '#include <cstdlib>',
        '#include "util.h"',
        'constexpr unsigned BLOCK_SIZE = {block_size};'.format(**env),
        'constexpr unsigned PAGE_SIZE = {page_size};'.format(**env),
        'constexpr uint64_t STAT_PRINTING_PERIOD = {heartbeat_frequency};'.format(**env),
        'constexpr std::size_t NUM_CPUS = {num_cores};'.format(**env),
        'constexpr auto LOG2_BLOCK_SIZE = champsim::lg2(BLOCK_SIZE);',
        'constexpr auto LOG2_PAGE_SIZE = champsim::lg2(PAGE_SIZE);',

        'constexpr uint64_t DRAM_IO_FREQ = {io_freq};'.format(**pmem),
        'constexpr std::size_t DRAM_CHANNELS = {channels};'.format(**pmem),
        'constexpr std::size_t DRAM_RANKS = {ranks};'.format(**pmem),
        'constexpr std::size_t DRAM_BANKS = {banks};'.format(**pmem),
        'constexpr std::size_t DRAM_ROWS = {rows};'.format(**pmem),
        'constexpr std::size_t DRAM_COLUMNS = {columns};'.format(**pmem),
        'constexpr std::size_t DRAM_CHANNEL_WIDTH = {channel_width};'.format(**pmem),
        'constexpr std::size_t DRAM_WQ_SIZE = {wq_size};'.format(**pmem),
        'constexpr std::size_t DRAM_RQ_SIZE = {rq_size};'.format(**pmem),
        '#endif')

