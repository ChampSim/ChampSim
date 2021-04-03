#!/usr/bin/env python3
import json
import sys,os
import itertools
import functools
import operator
import copy
from collections import ChainMap

constants_header_name = 'inc/champsim_constants.h'
instantiation_file_name = 'src/core_inst.cc'
config_cache_name = '.champsimconfig_cache'

###
# Begin format strings
###

dcn_fmtstr = 'cpu{}_{}' # default cache name format string

cache_fmtstr = 'CACHE {name}("{name}", {frequency}, {fill_level}, {sets}, {ways}, {wq_size}, {rq_size}, {pq_size}, {mshr_size}, {hit_latency}, {fill_latency}, {max_read}, {max_write}, {prefetch_as_load:b}, {virtual_prefetch:b}, {wq_check_full_addr:b}, {lower_level}, &CACHE::{prefetcher_initialize}, &CACHE::{prefetcher_cache_operate}, &CACHE::{prefetcher_cache_fill}, &CACHE::{prefetcher_cycle_operate}, &CACHE::{prefetcher_final_stats}, &CACHE::{replacement_initialize}, &CACHE::{replacement_find_victim}, &CACHE::{replacement_update_replacement_state}, &CACHE::{replacement_replacement_final_stats});\n'
ptw_fmtstr = 'PageTableWalker {name}("{name}", {fill_level}, {pscl5_set}, {pscl5_way}, {pscl4_set}, {pscl4_way}, {pscl3_set}, {pscl3_way}, {pscl2_set}, {pscl2_way}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, {lower_level});\n'

cpu_fmtstr = 'O3_CPU cpu{index}_inst({index}, {frequency}, {DIB[sets]}, {DIB[ways]}, {DIB[window_size]}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{ITLB}, &{DTLB}, &{L1I}, &{L1D}, &{PTW}, &O3_CPU::{bpred_initialize}, &O3_CPU::{bpred_last_result}, &O3_CPU::{bpred_predict}, &O3_CPU::{btb_initialize}, &O3_CPU::{btb_update}, &O3_CPU::{btb_predict}, &O3_CPU::{iprefetcher_initialize}, &O3_CPU::{iprefetcher_branch_operate}, &O3_CPU::{iprefetcher_cache_operate}, &O3_CPU::{iprefetcher_cache_fill}, &O3_CPU::{iprefetcher_cycle_operate}, &O3_CPU::{iprefetcher_final_stats});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER DRAM({attrs[frequency]});\n'
vmem_fmtstr = 'VirtualMemory vmem(NUM_CPUS, {attrs[size]}, PAGE_SIZE, {attrs[num_levels]}, 1);\n'

module_make_fmtstr = '{1}/%.o: CFLAGS += -I{1}\n{1}/%.o: CXXFLAGS += -I{1}\n{1}/%.o: CXXFLAGS += {2}\nobj/{0}: $(patsubst %.cc,%.o,$(wildcard {1}/*.cc)) $(patsubst %.c,%.o,$(wildcard {1}/*.c))\n\t@mkdir -p $(dir $@)\n\tar -rcs $@ $^\n\n'

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

default_root = { 'executable_name': 'bin/champsim', 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1, 'DIB': {}, 'L1I': {}, 'L1D': {}, 'L2C': {}, 'ITLB': {}, 'DTLB': {}, 'STLB': {}, 'LLC': {}, 'physical_memory': {}, 'virtual_memory': {}}

# Read the config file
if len(sys.argv) >= 2:
    with open(sys.argv[1]) as rfp:
        config_file = ChainMap(json.load(rfp), default_root)
else:
    print("No configuration specified. Building default ChampSim with no prefetching.")
    config_file = ChainMap(default_root)

default_core = { 'frequency' : 4000, 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetcher': 'no_instr', 'replacement': 'lru'}
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetcher': 'no', 'replacement': 'lru'}
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetcher': 'no', 'replacement': 'lru'}
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetcher': 'no', 'replacement': 'lru'}
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetcher': 'no', 'replacement': 'lru'}
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetcher': 'no', 'replacement': 'lru'}
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 16, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'fill_latency': 1, 'max_read': config_file['num_cores'], 'max_write': config_file['num_cores'], 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetcher': 'no', 'replacement': 'lru', 'name': 'LLC', 'lower_level': 'DRAM' }
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

config_file['physical_memory'] = ChainMap(config_file['physical_memory'], default_pmem.copy())
config_file['virtual_memory'] = ChainMap(config_file['virtual_memory'], default_vmem.copy())

cores = config_file.get('ooo_cpu', [{}])

# Index the cache array by names
caches = {c['name']: c for c in config_file.get('cache',[])}

# Default branch predictor and BTB
for i in range(len(cores)):
    cores[i] = ChainMap(cores[i], copy.deepcopy(default_root), default_core.copy())
    cores[i]['DIB'] = ChainMap(cores[i]['DIB'], config_file['DIB'].copy(), default_dib.copy())

# Copy or trim cores as necessary to fill out the specified number of cores
original_size = len(cores)
if original_size <= config_file['num_cores']:
    for i in range(original_size, config_file['num_cores']):
        cores.append(copy.deepcopy(cores[(i-1) % original_size]))
else:
    cores = config_file[:(config_file['num_cores'] - original_size)]

# Append LLC to cache array
# LLC operates at maximum freqency of cores, if not already specified
caches['LLC'] = ChainMap(caches.get('LLC',{}), config_file['LLC'].copy(), {'frequency': max(cpu['frequency'] for cpu in cores)}, default_llc.copy())

# If specified in the core, move definition to cache array
for i, cpu in enumerate(cores):
    # Assign defaults that are unique per core
    for cache_name in ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB'):
        if isinstance(cpu[cache_name], dict):
            cpu[cache_name] = ChainMap(cpu[cache_name], {'name': dcn_fmtstr.format(i,cache_name)}, config_file[cache_name].copy())
            caches[cpu[cache_name]['name']] = cpu[cache_name]
            cpu[cache_name] = cpu[cache_name]['name']

# Assign defaults that are unique per core
for cpu in cores:
    cpu['PTW'] = ChainMap(cpu.get('PTW',{}), config_file.get('PTW', {}), {'name': dcn_fmtstr.format(i,'PTW'), 'frequency': cpu['frequency'], 'lower_level': cpu['L1D']}, default_ptw.copy())
    caches[cpu['L1I']] = ChainMap(caches[cpu['L1I']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C']}, default_l1i.copy())
    caches[cpu['L1D']] = ChainMap(caches[cpu['L1D']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C']}, default_l1d.copy())
    caches[cpu['ITLB']] = ChainMap(caches[cpu['ITLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_itlb.copy())
    caches[cpu['DTLB']] = ChainMap(caches[cpu['DTLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_dtlb.copy())

    # L2C
    cache_name = caches[cpu['L1D']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': 'LLC'}, default_l2c.copy())

    # STLB
    cache_name = caches[cpu['DTLB']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': cpu['PTW']['name']}, default_l2c.copy())

# Remove caches that are inaccessible
accessible = [False]*len(caches)
for i,ll in enumerate(caches.values()):
    accessible[i] |= any(ul['lower_level'] == ll['name'] for ul in caches.values()) # The cache is accessible from another cache
    accessible[i] |= any(ll['name'] in [cpu['L1I'], cpu['L1D'], cpu['ITLB'], cpu['DTLB']] for cpu in cores) # The cache is accessible from a core
caches = dict(itertools.compress(caches.items(), accessible))

# Establish latencies in caches
for cache in caches.values():
    cache['hit_latency'] = cache.get('hit_latency') or (cache['latency'] - cache['fill_latency'])

# Scale frequencies
config_file['physical_memory']['io_freq'] = config_file['physical_memory']['frequency'] # Save value
freqs = list(itertools.chain(
    [cpu['frequency'] for cpu in cores],
    [cache['frequency'] for cache in caches.values()],
    (config_file['physical_memory']['frequency'],)
))
freqs = [max(freqs)/x for x in freqs]
for freq,src in zip(freqs, itertools.chain(cores, caches.values(), (config_file['physical_memory'],))):
    src['frequency'] = freq

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

# Associate modules with paths
libfilenames = {}

for cache in caches.values():
    # Resolve cache replacment function names
    if cache['replacement'] is not None:
        cache['replacement_initialize'] = 'repl_' + os.path.basename(cache['replacement']) + '_initialize'
        cache['replacement_find_victim'] = 'repl_' + os.path.basename(cache['replacement']) + '_victim'
        cache['replacement_update_replacement_state'] = 'repl_' + os.path.basename(cache['replacement']) + '_update'
        cache['replacement_replacement_final_stats'] = 'repl_' + os.path.basename(cache['replacement']) + '_final_stats'

        fname = 'replacement/' + cache['replacement']
        if not os.path.exists(fname):
            fname = os.path.normpath(os.path.expanduser(cache['replacement']))
        opts = ''
        opts += ' -Dinitialize_replacement=' + cache['replacement_initialize']
        opts += ' -Dfind_victim=' + cache['replacement_find_victim']
        opts += ' -Dupdate_replacement_state=' + cache['replacement_update_replacement_state']
        opts += ' -Dreplacement_final_stats=' + cache['replacement_replacement_final_stats']
        libfilenames['repl_' + os.path.basename(cache['replacement']) + '.a'] = (fname, opts)

    # Resolve prefetcher function names
    if cache['prefetcher'] is not None:
        cache['prefetcher_initialize'] = 'pref_' + os.path.basename(cache['prefetcher']) + '_initialize'
        cache['prefetcher_cache_operate'] = 'pref_' + os.path.basename(cache['prefetcher']) + '_cache_operate'
        cache['prefetcher_cache_fill'] = 'pref_' + os.path.basename(cache['prefetcher']) + '_cache_fill'
        cache['prefetcher_cycle_operate'] = 'pref_' + os.path.basename(cache['prefetcher']) + '_cycle_operate'
        cache['prefetcher_final_stats'] = 'pref_' + os.path.basename(cache['prefetcher']) + '_final_stats'

        fname = 'prefetcher/' + cache['prefetcher']
        if not os.path.exists(fname):
            fname = os.path.normpath(os.path.expanduser(cache['prefetcher']))
        opts = ''
        # These function names should be used in future designs
        opts += ' -Dprefetcher_initialize=' + cache['prefetcher_initialize']
        opts += ' -Dprefetcher_cache_operate=' + cache['prefetcher_cache_operate']
        opts += ' -Dprefetcher_cache_fill=' + cache['prefetcher_cache_fill']
        opts += ' -Dprefetcher_cycle_operate=' + cache['prefetcher_cycle_operate']
        opts += ' -Dprefetcher_final_stats=' + cache['prefetcher_final_stats']
        # These function names are deprecated, but we still permit them
        opts += ' -Dl1d_prefetcher_initialize=' + cache['prefetcher_initialize']
        opts += ' -Dl2c_prefetcher_initialize=' + cache['prefetcher_initialize']
        opts += ' -Dllc_prefetcher_initialize=' + cache['prefetcher_initialize']
        opts += ' -Dl1d_prefetcher_operate=' + cache['prefetcher_cache_operate']
        opts += ' -Dl2c_prefetcher_operate=' + cache['prefetcher_cache_operate']
        opts += ' -Dllc_prefetcher_operate=' + cache['prefetcher_cache_operate']
        opts += ' -Dl1d_prefetcher_cache_fill=' + cache['prefetcher_cache_fill']
        opts += ' -Dl2c_prefetcher_cache_fill=' + cache['prefetcher_cache_fill']
        opts += ' -Dllc_prefetcher_cache_fill=' + cache['prefetcher_cache_fill']
        opts += ' -Dl1d_prefetcher_final_stats=' + cache['prefetcher_final_stats']
        opts += ' -Dl2c_prefetcher_final_stats=' + cache['prefetcher_final_stats']
        opts += ' -Dllc_prefetcher_final_stats=' + cache['prefetcher_final_stats']
        libfilenames['pref_' + os.path.basename(cache['prefetcher']) + '.a'] = (fname, opts)

for cpu in cores:
    # Resolve branch predictor function names
    if cpu['branch_predictor'] is not None:
        cpu['bpred_initialize'] = 'bpred_' + os.path.basename(cpu['branch_predictor']) + '_initialize'
        cpu['bpred_last_result'] = 'bpred_' + os.path.basename(cpu['branch_predictor']) + '_last_result'
        cpu['bpred_predict'] = 'bpred_' + os.path.basename(cpu['branch_predictor']) + '_predict'

        fname = 'branch/' + cpu['branch_predictor']
        if not os.path.exists(fname):
            fname = os.path.normpath(os.path.expanduser(cpu['branch_predictor']))
        opts = ''
        opts += ' -Dinitialize_branch_predictor=' + cpu['bpred_initialize']
        opts += ' -Dlast_branch_result=' + cpu['bpred_last_result']
        opts += ' -Dpredict_branch=' + cpu['bpred_predict']
        libfilenames['bpred_' + os.path.basename(cpu['branch_predictor']) + '.a'] = (fname, opts)

    # Resolve BTB function names
    if cpu['btb'] is not None:
        cpu['btb_initialize'] = 'btb_' + os.path.basename(cpu['btb']) + '_initialize'
        cpu['btb_update'] = 'btb_' + os.path.basename(cpu['btb']) + '_update'
        cpu['btb_predict'] = 'btb_' + os.path.basename(cpu['btb']) + '_predict'

        fname = 'btb/' + cpu['btb']
        if not os.path.exists(fname):
            fname = os.path.normpath(os.path.expanduser(cpu['btb']))
        opts = ''
        opts += ' -Dinitialize_btb=' + cpu['btb_initialize']
        opts += ' -Dupdate_btb=' + cpu['btb_update']
        opts += ' -Dbtb_prediction=' + cpu['btb_predict']
        libfilenames['btb_' + os.path.basename(cpu['btb']) + '.a'] = (fname, opts)


    # Resolve instruction prefetching function names
    cpu['iprefetcher_initialize'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_initialize'
    cpu['iprefetcher_branch_operate'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_branch_operate'
    cpu['iprefetcher_cache_operate'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_cache_operate'
    cpu['iprefetcher_cycle_operate'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_cycle_operate'
    cpu['iprefetcher_cache_fill'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_cache_fill'
    cpu['iprefetcher_final_stats'] = 'pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '_final_stats'

    if os.path.exists('prefetcher/' + caches[cpu['L1I']]['prefetcher']):
        fname = 'prefetcher/' + caches[cpu['L1I']]['prefetcher']
    else:
        fname = os.path.normpath(os.path.expanduser(caches[cpu['L1I']]['prefetcher']))
    opts = ''
    # These function names should be used in future designs
    opts += ' -Dprefetcher_initialize=' + cpu['iprefetcher_initialize']
    opts += ' -Dprefetcher_branch_operate=' + cpu['iprefetcher_branch_operate']
    opts += ' -Dprefetcher_cache_operate=' + cpu['iprefetcher_cache_operate']
    opts += ' -Dprefetcher_cycle_operate=' + cpu['iprefetcher_cycle_operate']
    opts += ' -Dprefetcher_cache_fill=' + cpu['iprefetcher_cache_fill']
    opts += ' -Dprefetcher_final_stats=' + cpu['iprefetcher_final_stats']
    # These function names are deprecated, but we still permit them
    opts += ' -Dl1i_prefetcher_initialize=' + cpu['iprefetcher_initialize']
    opts += ' -Dl1i_prefetcher_branch_operate=' + cpu['iprefetcher_branch_operate']
    opts += ' -Dl1i_prefetcher_cache_operate=' + cpu['iprefetcher_cache_operate']
    opts += ' -Dl1i_prefetcher_cycle_operate=' + cpu['iprefetcher_cycle_operate']
    opts += ' -Dl1i_prefetcher_cache_fill=' + cpu['iprefetcher_cache_fill']
    opts += ' -Dl1i_prefetcher_final_stats=' + cpu['iprefetcher_final_stats']
    libfilenames['pref_' + os.path.basename(caches[cpu['L1I']]['prefetcher']) + '.a'] = (fname, opts)

    # Override instruction prefetcher function names in the cache
    caches[cpu['L1I']]['prefetcher_initialize'] = 'cpu_redir_ipref_initialize'
    caches[cpu['L1I']]['prefetcher_cache_operate'] = 'cpu_redir_ipref_operate'
    caches[cpu['L1I']]['prefetcher_cache_fill'] = 'cpu_redir_ipref_fill'
    caches[cpu['L1I']]['prefetcher_cycle_operate'] = 'cpu_redir_ipref_cycle_operate'
    caches[cpu['L1I']]['prefetcher_final_stats'] = 'cpu_redir_ipref_final_stats'

# Assert module paths exist
for path,_ in libfilenames.values():
    if not os.path.exists(path):
        print('Path "' + path + '" does not exist. Exiting...')
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
for i in range(len(cores)):
    ptws[cores[i]['PTW']['name']] = cores[i]['PTW']
    cores[i]['PTW'] = cores[i]['PTW']['name']

memory_system = dict(**caches, **ptws)

# Give each element a fill level
active_keys = list(itertools.chain.from_iterable((cpu['ITLB'], cpu['DTLB'], cpu['L1I'], cpu['L1D']) for cpu in cores))
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

    for i,cpu in enumerate(cores):
        wfp.write(cpu_fmtstr.format(index=i, **cpu))

    wfp.write('std::array<O3_CPU*, NUM_CPUS> ooo_cpu {\n')
    for i in range(len(cores)):
        if i > 0:
            wfp.write(',\n')
        wfp.write('&cpu{}_inst'.format(i))
    wfp.write('\n};\n')

    wfp.write('std::array<CACHE*, NUM_CACHES> caches {\n')
    for i,cache in enumerate(caches.values()):
        if i > 0:
            wfp.write(',')
        wfp.write('&{name}'.format(**cache))
    wfp.write('\n};\n')

    wfp.write('std::array<champsim::operable*, NUM_OPERABLES> operables {\n')
    for i in range(len(cores)):
        wfp.write('&cpu{}_inst, '.format(i))
    wfp.write('\n')

    for cache in memory_system:
        wfp.write('&{name}, '.format(**cache))

    wfp.write('\n&DRAM')
    wfp.write('\n};\n')

# Core modules file
bpred_inits        = {c['bpred_initialize'] for c in cores}
bpred_last_results = {c['bpred_last_result'] for c in cores}
bpred_predicts     = {c['bpred_predict'] for c in cores}
btb_inits          = {c['btb_initialize'] for c in cores}
btb_updates        = {c['btb_update'] for c in cores}
btb_predicts       = {c['btb_predict'] for c in cores}
ipref_inits        = {c['iprefetcher_initialize'] for c in cores}
ipref_branch_ops   = {c['iprefetcher_branch_operate'] for c in cores}
ipref_cache_ops    = {c['iprefetcher_cache_operate'] for c in cores}
ipref_cycle_ops    = {c['iprefetcher_cycle_operate'] for c in cores}
ipref_fill         = {c['iprefetcher_cache_fill'] for c in cores}
ipref_finals       = {c['iprefetcher_final_stats'] for c in cores}
with open('inc/ooo_cpu_modules.inc', 'wt') as wfp:
    for i in bpred_inits:
        wfp.write('void ' + i + '();\n')

    for l in bpred_last_results:
        wfp.write('void ' + l + '(uint64_t, uint64_t, uint8_t, uint8_t);\n')

    for p in bpred_predicts:
        wfp.write('uint8_t ' + p + '(uint64_t, uint64_t, uint8_t, uint8_t);\n')

    for i in btb_inits:
        wfp.write('void ' + i + '();\n')

    for u in btb_updates:
        wfp.write('void ' + u + '(uint64_t, uint64_t, uint8_t, uint8_t);\n')

    for p in btb_predicts:
        wfp.write('std::pair<uint64_t, uint8_t> ' + p + '(uint64_t, uint8_t);\n')

    for i in ipref_inits:
        wfp.write('void ' + i + '();\n')

    for b in ipref_branch_ops:
        wfp.write('void ' + b + '(uint64_t, uint8_t, uint64_t);\n')

    for v in ipref_cache_ops:
        wfp.write('uint32_t ' + v + '(uint64_t, uint8_t, uint8_t, uint32_t);\n')

    for c in ipref_cycle_ops:
        wfp.write('void ' + c + '();\n')

    for u in ipref_fill:
        wfp.write('uint32_t ' + u + '(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);\n')

    for f in ipref_finals:
        wfp.write('void ' + f + '();\n')

# Cache modules file
repl_inits   = {c['replacement_initialize'] for c in caches.values()}
repl_victims = {c['replacement_find_victim'] for c in caches.values()}
repl_updates = {c['replacement_update_replacement_state'] for c in caches.values()}
repl_finals  = {c['replacement_replacement_final_stats'] for c in caches.values()}
# Exclude redirected L1I functions
pref_inits   = {c['prefetcher_initialize'] for c in caches.values() if c['prefetcher_initialize'].startswith('pref')}
pref_ops     = {c['prefetcher_cache_operate'] for c in caches.values() if c['prefetcher_cache_operate'].startswith('pref')}
pref_fill    = {c['prefetcher_cache_fill'] for c in caches.values() if c['prefetcher_cache_fill'].startswith('pref')}
pref_cycles  = {c['prefetcher_cycle_operate'] for c in caches.values() if c['prefetcher_cycle_operate'].startswith('pref')}
pref_finals  = {c['prefetcher_final_stats'] for c in caches.values() if c['prefetcher_final_stats'].startswith('pref')}
with open('inc/cache_modules.inc', 'wt') as wfp:
    for i in repl_inits:
        wfp.write('void ' + i + '();\n')

    for v in repl_victims:
        wfp.write('uint32_t ' + v + '(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t);\n')

    for u in repl_updates:
        wfp.write('void ' + u + '(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t);\n')

    for f in repl_finals:
        wfp.write('void ' + f + '();\n')

    for i in pref_inits:
        wfp.write('void ' + i + '();\n')

    for v in pref_ops:
        wfp.write('uint32_t ' + v + '(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t);\n')

    for u in pref_fill:
        wfp.write('uint32_t ' + u + '(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);\n')

    for v in pref_cycles:
        wfp.write('void ' + v + '();\n')

    for f in pref_finals:
        wfp.write('void ' + f + '();\n')

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
    wfp.write('#define NUM_CACHES ' + str(len(caches)) + 'u\n')
    wfp.write('#define NUM_OPERABLES ' + str(len(cores) + len(memory_system) + 1) + 'u\n')

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
    wfp.write('clean: \n')
    wfp.write('\t$(RM) ' + constants_header_name + '\n')
    wfp.write('\t$(RM) ' + instantiation_file_name + '\n')
    wfp.write('\t$(RM) ' + 'inc/cache_modules.inc' + '\n')
    wfp.write('\t$(RM) ' + 'inc/ooo_cpu_modules.inc' + '\n')
    wfp.write('\t find . -name \*.o -delete\n\t find . -name \*.d -delete\n\t $(RM) -r obj\n\n')
    for v in libfilenames.values():
        wfp.write('\t find {0} -name \*.o -delete\n\t find {0} -name \*.d -delete\n'.format(*v))
    wfp.write('\n')
    wfp.write(config_file['executable_name'] + ': $(patsubst %.cc,%.o,$(wildcard src/*.cc)) ' + ' '.join('obj/' + k for k in libfilenames) + '\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    for k,v in libfilenames.items():
        wfp.write(module_make_fmtstr.format(k, *v))

    wfp.write('-include $(wildcard src/*.d)\n')
    for v in libfilenames.values():
        wfp.write('-include $(wildcard {0}/*.d)\n'.format(*v))
    wfp.write('\n')

# Configuration cache
with open(config_cache_name, 'wt') as wfp:
    json.dump(libfilenames, wfp)

