#!/usr/bin/env python3
import json
import sys,os
import itertools
import functools
import operator
import copy

# Read the config file
if len(sys.argv) >= 2:
    with open(sys.argv[1]) as rfp:
        config_file = json.load(rfp)
else:
    print("No configuration specified. Building default ChampSim with no prefetching.")
    config_file = {}

def merge_dicts(*dicts):
    z = dicts[0].copy()
    for d in dicts[1:]:
        z.update(d)
    return z

constants_header_name = 'inc/champsim_constants.h'
instantiation_file_name = 'src/core_inst.cc'
config_cache_name = '.champsimconfig_cache'

###
# Begin format strings
###

dcn_fmtstr = 'cpu{}_{}' # default cache name format string

cache_fmtstr = 'CACHE {name}("{name}", {frequency}, {sets}, {ways}, {wq_size}, {rq_size}, {pq_size}, {mshr_size}, {hit_latency}, {fill_latency}, {max_read}, {max_write}, {prefetch_as_load:b}, {virtual_prefetch:b}, {lower_level});\n'
ptw_fmtstr = 'PageTableWalker {name}("{name}", {pscl5_set}, {pscl5_way}, {pscl4_set}, {pscl4_way}, {pscl3_set}, {pscl3_way}, {pscl2_set}, {pscl2_way}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, {lower_level});\n'

cpu_fmtstr = 'O3_CPU cpu{cpu}_inst({cpu}, {attrs[frequency]}, {attrs[DIB][sets]}, {attrs[DIB][ways]}, {attrs[DIB][window_size]}, {attrs[ifetch_buffer_size]}, {attrs[dispatch_buffer_size]}, {attrs[decode_buffer_size]}, {attrs[rob_size]}, {attrs[lq_size]}, {attrs[sq_size]}, {attrs[fetch_width]}, {attrs[decode_width]}, {attrs[dispatch_width]}, {attrs[scheduler_size]}, {attrs[execute_width]}, {attrs[lq_width]}, {attrs[sq_width]}, {attrs[retire_width]}, {attrs[mispredict_penalty]}, {attrs[decode_latency]}, {attrs[dispatch_latency]}, {attrs[schedule_latency]}, {attrs[execute_latency]}, &{attrs[ITLB]}, &{attrs[DTLB]}, &{attrs[L1I]}, &{attrs[L1D]}, &{attrs[PTW]});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER DRAM({attrs[frequency]});\n'
vmem_fmtstr = 'VirtualMemory vmem(NUM_CPUS, {attrs[size]}, PAGE_SIZE, {attrs[num_levels]}, 1);\n'

module_make_fmtstr = '{1}/%.o: CFLAGS += -I{1}\n{1}/%.o: CXXFLAGS += -I{1}\nobj/{0}: $(patsubst %.cc,%.o,$(wildcard {1}/*.cc)) $(patsubst %.c,%.o,$(wildcard {1}/*.c))\n\t@mkdir -p $(dir $@)\n\tar -rcs $@ $^\n\n'

define_fmtstr = '#define {{names[{name}]}} {{config[{name}]}}u\n'
define_nonint_fmtstr = '#define {{names[{name}]}} {{config[{name}]}}\n'
define_log_fmtstr = '#define LOG2_{{names[{name}]}} lg2({{names[{name}]}})\n'

###
# Begin named constants
###

const_names = {
    'block_size': 'BLOCK_SIZE',
    'page_size': 'PAGE_SIZE',
    'heartbeat_frequency': 'STAT_PRINTING_PERIOD',
    'num_cores': 'NUM_CPUS',
    'physical_memory': {
        'io_freq': 'DRAM_IO_FREQ',
        'channels': 'DRAM_CHANNELS',
        'ranks': 'DRAM_RANKS',
        'banks': 'DRAM_BANKS',
        'rows': 'DRAM_ROWS',
        'columns': 'DRAM_COLUMNS',
        'row_size': 'DRAM_ROW_SIZE',
        'channel_width': 'DRAM_CHANNEL_WIDTH',
        'wq_size': 'DRAM_WQ_SIZE',
        'rq_size': 'DRAM_RQ_SIZE',
        'tRP': 'tRP_DRAM_NANOSECONDS',
        'tRCD': 'tRCD_DRAM_NANOSECONDS',
        'tCAS': 'tCAS_DRAM_NANOSECONDS',
        'turn_around_time': 'DBUS_TURN_AROUND_NANOSECONDS'
    }
}

###
# Begin default core model definition
###

default_root = { 'executable_name': 'bin/champsim', 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1, 'ooo_cpu': [{}] }
config_file = merge_dicts(default_root, config_file) # doing this early because LLC dimensions depend on it

default_core = { 'frequency' : 4000, 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'prefetcher': 'no_l1i' }
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'prefetcher': 'no_l1d' }
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'prefetcher': 'no_l2c' }
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True }
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False }
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False }
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 16, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'fill_latency': 1, 'max_read': config_file['num_cores'], 'max_write': config_file['num_cores'], 'prefetch_as_load': False, 'virtual_prefetch': False, 'prefetcher': 'no_llc', 'replacement': 'lru_llc', 'name': 'LLC', 'lower_level': 'DRAM' }
default_pmem = { 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'row_size': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'size': 8589934592, 'num_levels': 5 }
default_ptw = { 'pscl5_set' : 1, 'pscl5_way' : 2, 'pscl4_set' : 1, 'pscl4_way': 4, 'pscl3_set' : 2, 'pscl3_way' : 4, 'pscl2_set' : 4, 'pscl2_way': 8, 'ptw_rq_size': 16, 'ptw_mshr_size': 5, 'ptw_max_read': 2, 'ptw_max_write': 2}
###
# Ensure directories are present
###

os.makedirs(os.path.dirname(config_file['executable_name']), exist_ok=True)
os.makedirs(os.path.dirname(instantiation_file_name), exist_ok=True)
os.makedirs(os.path.dirname(constants_header_name), exist_ok=True)
os.makedirs('obj', exist_ok=True)

###
# Establish default optional values
###

config_file['physical_memory'] = merge_dicts(default_pmem, config_file.get('physical_memory',{}))
config_file['virtual_memory'] = merge_dicts(default_vmem, config_file.get('virtual_memory',{}))

# Default branch predictor and BTB
for i in range(len(config_file['ooo_cpu'])):
    config_file['ooo_cpu'][i] = merge_dicts(default_core, {'branch_predictor': config_file['branch_predictor']} if 'branch_predictor' in config_file else {}, config_file['ooo_cpu'][i])
    config_file['ooo_cpu'][i] = merge_dicts(default_core, {'btb': config_file['btb']} if 'btb' in config_file else {}, config_file['ooo_cpu'][i])
    config_file['ooo_cpu'][i]['DIB'] = merge_dicts(default_dib, config_file.get('DIB', {}), config_file['ooo_cpu'][i].get('DIB',{}))

# Copy or trim cores as necessary to fill out the specified number of cores
original_size = len(config_file['ooo_cpu'])
if original_size <= config_file['num_cores']:
    for i in range(original_size, config_file['num_cores']):
        config_file['ooo_cpu'].append(copy.deepcopy(config_file['ooo_cpu'][(i-1) % original_size]))
else:
    config_file['ooo_cpu'] = config_file[:(config_file['num_cores'] - original_size)]

# Default cache array
config_file['cache'] = config_file.get('cache', [])

# Index the cache array by names
config_file['cache'] = {c['name']: c for c in config_file['cache']}

# Append LLC to cache array
# LLC operates at maximum freqency of cores, if not already specified
config_file['cache']['LLC'] = merge_dicts(default_llc, {'name': 'LLC', 'frequency': max(cpu['frequency'] for cpu in config_file['ooo_cpu'])}, config_file.get('LLC',{}), config_file['cache'].get('LLC',{}))

# If specified in the core, move definition to cache array
for i, cpu in enumerate(config_file['ooo_cpu']):
    # Assign defaults that are unique per core
    for cache_name in ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB'):
        if isinstance(cpu.get(cache_name,{}), dict):
            cpu[cache_name] = merge_dicts({'name': dcn_fmtstr.format(i,cache_name)}, cpu.get(cache_name, {}))
            config_file['cache'][cpu[cache_name]['name']] = cpu[cache_name]
            cpu[cache_name] = cpu[cache_name]['name']

# Assign defaults that are unique per core
for i,cpu in enumerate(config_file['ooo_cpu']):
    percore_default = {'name': dcn_fmtstr.format(i,'PTW'), 'frequency': cpu['frequency'], 'lower_level': cpu['L1D']}
    config_file['ooo_cpu'][i]['PTW'] = merge_dicts(default_ptw, percore_default, config_file.get('PTW', {}), cpu.get('PTW',{}))

    # L1I
    percore_default = {'frequency': cpu['frequency'], 'lower_level': cpu['L2C']}
    config_file['cache'][cpu['L1I']] = merge_dicts(default_l1i, percore_default, config_file.get('L1I', {}), config_file['cache'][cpu['L1I']])

    # L1D
    percore_default = {'frequency': cpu['frequency'], 'lower_level': cpu['L2C']}
    config_file['cache'][cpu['L1D']] = merge_dicts(default_l1d, percore_default, config_file.get('L1D', {}), config_file['cache'][cpu['L1D']])

    # ITLB
    percore_default = {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}
    config_file['cache'][cpu['ITLB']] = merge_dicts(default_itlb, percore_default, config_file.get('ITLB', {}), config_file['cache'][cpu['ITLB']])

    # DTLB
    percore_default = {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}
    config_file['cache'][cpu['DTLB']] = merge_dicts(default_dtlb, percore_default, config_file.get('DTLB', {}), config_file['cache'][cpu['DTLB']])

    # L2C
    percore_default = {'frequency': cpu['frequency'], 'lower_level': 'LLC'}
    cache_name = config_file['cache'][cpu['L1D']]['lower_level']
    if cache_name != 'DRAM':
        config_file['cache'][cache_name] = merge_dicts(default_l2c, percore_default, config_file.get('L2C', {}), config_file['cache'][cache_name])

    # STLB
    percore_default = {'frequency': cpu['frequency'], 'lower_level': cpu['PTW']['name']}
    cache_name = config_file['cache'][cpu['DTLB']]['lower_level']
    if cache_name != 'DRAM':
        config_file['cache'][cache_name] = merge_dicts(default_l2c, percore_default, config_file.get('STLB', {}), config_file['cache'][cache_name])

# Remove caches that are inaccessible
accessible = [False]*len(config_file['cache'])
for i,ll in enumerate(config_file['cache'].values()):
    accessible[i] |= any(ul['lower_level'] == ll['name'] for ul in config_file['cache'].values()) # The cache is accessible from another cache
    accessible[i] |= any(ll['name'] in [cpu['L1I'], cpu['L1D'], cpu['ITLB'], cpu['DTLB']] for cpu in config_file['ooo_cpu']) # The cache is accessible from a core
config_file['cache'] = dict(itertools.compress(config_file['cache'].items(), accessible))

# Establish latencies in caches
# If not specified, hit and fill latencies are half of the total latency, where fill takes longer if the sum is odd.
for cache in config_file['cache'].values():
    cache['hit_latency'] = cache.get('hit_latency', cache['latency'] - cache['fill_latency'])

# Scale frequencies
config_file['physical_memory']['io_freq'] = config_file['physical_memory']['frequency'] # Save value
freqs = list(itertools.chain(
    [cpu['frequency'] for cpu in config_file['ooo_cpu']],
    [cache['frequency'] for cache in config_file['cache'].values()],
    (config_file['physical_memory']['frequency'],)
))
freqs = [max(freqs)/x for x in freqs]
for i,cpu in enumerate(config_file['ooo_cpu']):
    cpu['frequency'] = freqs[i]
for i,cache in enumerate(config_file['cache'].values()):
    cache['frequency'] = freqs[len(config_file['ooo_cpu'])+i]
config_file['physical_memory']['frequency'] = freqs[-1]

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

# Associate modules with paths
libfilenames = {}
for i,cpu in enumerate(config_file['ooo_cpu'][:1]):
    if config_file['cache'][cpu['L1I']]['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l1iprefetcher.a'] = 'prefetcher/' + config_file['cache'][cpu['L1I']]['prefetcher']
    if config_file['cache'][cpu['L1D']]['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l1dprefetcher.a'] = 'prefetcher/' + config_file['cache'][cpu['L1D']]['prefetcher']
    if config_file['cache'][config_file['cache'][cpu['L1D']]['lower_level']]['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l2cprefetcher.a'] = 'prefetcher/' + config_file['cache'][config_file['cache'][cpu['L1D']]['lower_level']]['prefetcher']
    if cpu['branch_predictor'] is not None:
        if os.path.exists('branch/' + cpu['branch_predictor']):
            libfilenames['cpu' + str(i) + 'branch_predictor.a'] = 'branch/' + cpu['branch_predictor']
        elif os.path.exists(os.path.normpath(os.path.expanduser(cpu['branch_predictor']))):
            libfilenames['cpu' + str(i) + 'branch_predictor.a'] = os.path.normpath(os.path.expanduser(cpu['branch_predictor']))
        else:
            print('Path to branch predictor does not exist. Exiting...')
            sys.exit(1)

    if cpu['btb'] is not None:
        if os.path.exists('btb/' + cpu['btb']):
            libfilenames['cpu' + str(i) + 'btb.a'] = 'btb/' + cpu['btb']
        elif os.path.exists(os.path.normpath(os.path.expanduser(cpu['btb']))):
            libfilenames['cpu' + str(i) + 'btb.a'] = os.path.normpath(os.path.expanduser(cpu['btb']))
        else:
            print('Path to BTB does not exist. Exiting...')
            sys.exit(1)

if config_file['cache']['LLC']['prefetcher'] is not None:
    if os.path.exists('prefetcher/' + config_file['cache']['LLC']['prefetcher']):
        libfilenames['cpu' + str(i) + 'llprefetcher.a'] = 'prefetcher/' + config_file['cache']['LLC']['prefetcher']
    elif os.path.exists(os.path.normpath(os.path.expanduser(config_file['cache']['LLC']['prefetcher']))):
        libfilenames['cpu' + str(i) + 'llprefetcher.a'] = os.path.normpath(os.path.expanduser(config_file['cache']['LLC']['prefetcher']))
    else:
        print('Path to LLC prefetcher does not exist. Exiting...')
        sys.exit(1)

if config_file['cache']['LLC']['replacement'] is not None:
    if os.path.exists('replacement/' + config_file['cache']['LLC']['replacement']):
        libfilenames['cpu' + str(i) + 'llreplacement.a'] = 'replacement/' + config_file['cache']['LLC']['replacement']
    elif os.path.exists(os.path.normpath(os.path.expanduser(config_file['cache']['LLC']['replacement']))):
        libfilenames['cpu' + str(i) + 'llreplacement.a'] = os.path.normpath(os.path.expanduser(config_file['cache']['LLC']['replacement']))
    else:
        print('Path to LLC replacement does not exist. Exiting...')
        sys.exit(1)

# Check cache of previous configuration
if os.path.exists(config_cache_name):
    with open(config_cache_name) as rfp:
        config_cache = json.load(rfp)
else:
    config_cache = {k:'' for k in libfilenames}

# Prune modules whose configurations have changed (force make to rebuild it)
for f in os.listdir('obj'):
    if f in libfilenames and f in config_cache and config_cache[f] != libfilenames[f]:
        os.remove('obj/' + f)

###
# Perform final preparations for file writing
###

# Add PTW to memory system
ptws = {}
for i in range(len(config_file['ooo_cpu'])):
    ptws[config_file['ooo_cpu'][i]['PTW']['name']] = config_file['ooo_cpu'][i]['PTW']
    config_file['ooo_cpu'][i]['PTW'] = config_file['ooo_cpu'][i]['PTW']['name']

memory_system = dict(config_file['cache'], **ptws)

# Give each element a fill level
active_keys = list(itertools.chain.from_iterable((cpu['ITLB'], cpu['DTLB'], cpu['L1I'], cpu['L1D']) for cpu in config_file['ooo_cpu']))
for k in active_keys:
    memory_system[k]['fill_level'] = 1

for fill_level in range(1,len(memory_system)+1):
    for k in active_keys:
        if memory_system[k]['lower_level'] != 'DRAM':
            memory_system[memory_system[k]['lower_level']]['fill_level'] = max(memory_system[memory_system[k]['lower_level']].get('fill_level',0), fill_level+1)
    active_keys = [memory_system[k]['lower_level'] for k in active_keys if memory_system[k]['lower_level'] != 'DRAM']

# Remove name index
memory_system = list(memory_system.values())

memory_system.sort(key=operator.itemgetter('fill_level'), reverse=True)

# Check for lower levels in the array
for i in reversed(range(len(memory_system))):
    ul = memory_system[i]
    if ul['lower_level'] != 'DRAM':
        if not any((ul['lower_level'] == ll['name']) for ll in memory_system[:i]):
            print('Could not find cache "' + ul['lower_level'] + '" in cache array. Exiting...')
            sys.exit(1)

# prune Nones
for elem in memory_system:
    if elem['lower_level'] is not None:
        elem['lower_level'] = '&'+elem['lower_level'] # append address operator for C++

###
# Begin file writing
###

# Instantiation file
with open(instantiation_file_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#include "cache.h"\n')
    wfp.write('#include "champsim.h"\n')
    wfp.write('#include "dram_controller.h"\n')
    wfp.write('#include "ooo_cpu.h"\n')
    wfp.write('#include "vmem.h"\n')
    wfp.write('#include "operable.h"\n')
    wfp.write('#include "' + os.path.basename(constants_header_name) + '"\n')
    wfp.write('#include <array>\n')
    wfp.write('#include <vector>\n')

    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')
    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    for elem in memory_system:
        if 'pscl5_set' in elem:
            wfp.write(ptw_fmtstr.format(**elem))
        else:
            wfp.write(cache_fmtstr.format(**elem))

    for i,cpu in enumerate(config_file['ooo_cpu']):
        wfp.write(cpu_fmtstr.format(cpu=i, attrs=cpu))

    wfp.write('std::array<O3_CPU*, NUM_CPUS> ooo_cpu {\n')
    for i in range(len(config_file['ooo_cpu'])):
        if i > 0:
            wfp.write(',\n')
        wfp.write('&cpu{}_inst'.format(i))
    wfp.write('\n};\n')

    wfp.write('std::array<champsim::operable*, NUM_OPERABLES> operables {\n')
    for i in range(len(config_file['ooo_cpu'])):
        wfp.write('&cpu{}_inst, '.format(i))
    wfp.write('\n')

    for cache in memory_system:
        wfp.write('&{name}, '.format(**cache))

    wfp.write('\n&DRAM')
    wfp.write('\n};\n')

# Constants header
with open(constants_header_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#ifndef CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#define CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#include "util.h"\n')
    wfp.write(define_fmtstr.format(name='block_size').format(names=const_names, config=config_file))
    wfp.write(define_log_fmtstr.format(name='block_size').format(names=const_names, config=config_file))
    wfp.write(define_fmtstr.format(name='page_size').format(names=const_names, config=config_file))
    wfp.write(define_log_fmtstr.format(name='page_size').format(names=const_names, config=config_file))
    wfp.write(define_fmtstr.format(name='heartbeat_frequency').format(names=const_names, config=config_file))
    wfp.write(define_fmtstr.format(name='num_cores').format(names=const_names, config=config_file))
    wfp.write('#define NUM_OPERABLES ' + str(len(config_file['ooo_cpu']) + len(memory_system) + 1) + 'u\n')

    for k in const_names['physical_memory']:
        if k in ['tRP', 'tRCD', 'tCAS', 'turn_around_time']:
            wfp.write(define_nonint_fmtstr.format(name=k).format(names=const_names['physical_memory'], config=config_file['physical_memory']))
        else:
            wfp.write(define_fmtstr.format(name=k).format(names=const_names['physical_memory'], config=config_file['physical_memory']))
        if k in ['channels', 'ranks', 'banks', 'rows', 'columns']:
            wfp.write(define_log_fmtstr.format(name=k).format(names=const_names['physical_memory'], config=config_file['physical_memory']))

    wfp.write('#endif\n')

# Makefile
with open('Makefile', 'wt') as wfp:
    wfp.write('CC := ' + config_file.get('CC', 'gcc') + '\n')
    wfp.write('CXX := ' + config_file.get('CXX', 'g++') + '\n')
    wfp.write('CFLAGS := ' + config_file.get('CFLAGS', '-Wall -O3') + ' -std=gnu99\n')
    wfp.write('CXXFLAGS := ' + config_file.get('CXXFLAGS', '-Wall -O3') + ' -std=c++17\n')
    wfp.write('CPPFLAGS := ' + config_file.get('CPPFLAGS', '') + ' -Iinc -MMD -MP\n')
    wfp.write('LDFLAGS := ' + config_file.get('LDFLAGS', '') + '\n')
    wfp.write('LDLIBS := ' + config_file.get('LDLIBS', '') + '\n')
    wfp.write('\n')
    wfp.write('.phony: all clean\n\n')
    wfp.write('all: ' + config_file['executable_name'] + '\n\n')
    wfp.write('clean: \n\t find . -name \*.o -delete\n\t find . -name \*.d -delete\n\t $(RM) -r obj\n')
    for v in libfilenames.values():
        wfp.write('\t find {0} -name \*.o -delete\n\t find {0} -name \*.d -delete\n'.format(v))
    wfp.write('\n')
    wfp.write(config_file['executable_name'] + ': $(patsubst %.cc,%.o,$(wildcard src/*.cc)) ' + ' '.join('obj/' + k for k in libfilenames) + '\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    for kv in libfilenames.items():
        wfp.write(module_make_fmtstr.format(*kv))

    wfp.write('-include $(wildcard src/*.d)\n')
    for v in libfilenames.values():
        wfp.write('-include $(wildcard {0}/*.d)\n'.format(v))
    wfp.write('\n')

# Configuration cache
with open(config_cache_name, 'wt') as wfp:
    json.dump(libfilenames, wfp)

