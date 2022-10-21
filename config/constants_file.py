
def get_constants_file(env, pmem):
    retval = ''
    retval += '#ifndef CHAMPSIM_CONSTANTS_H\n'
    retval += '#define CHAMPSIM_CONSTANTS_H\n'
    retval += '#include <cstdlib>\n'
    retval += '#include "util.h"\n'
    retval += 'constexpr unsigned BLOCK_SIZE = {block_size};\n'.format(**env)
    retval += 'constexpr unsigned PAGE_SIZE = {page_size};\n'.format(**env)
    retval += 'constexpr uint64_t STAT_PRINTING_PERIOD = {heartbeat_frequency};\n'.format(**env)
    retval += 'constexpr std::size_t NUM_CPUS = {num_cores};\n'.format(**env)
    retval += 'constexpr auto LOG2_BLOCK_SIZE = lg2(BLOCK_SIZE);\n'
    retval += 'constexpr auto LOG2_PAGE_SIZE = lg2(PAGE_SIZE);\n'

    retval += 'constexpr uint64_t DRAM_IO_FREQ = {io_freq};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_CHANNELS = {channels};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_RANKS = {ranks};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_BANKS = {banks};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_ROWS = {rows};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_COLUMNS = {columns};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_CHANNEL_WIDTH = {channel_width};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_WQ_SIZE = {wq_size};\n'.format(**pmem)
    retval += 'constexpr std::size_t DRAM_RQ_SIZE = {rq_size};\n'.format(**pmem)
    retval += '#endif\n'

    return retval

