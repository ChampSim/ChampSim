
def get_constants_file(env, pmem):
    yield from (
        '#ifndef CHAMPSIM_CONSTANTS_H',
        '#define CHAMPSIM_CONSTANTS_H',
        '#include <cstdlib>',
        '#include "util/bits.h"',
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

