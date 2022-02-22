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

fname_translation_table = str.maketrans('./-','_DH')

###
# Begin format strings
###

cache_fmtstr = 'CACHE {name}("{name}", {frequency}, {fill_level}, {sets}, {ways}, {wq_size}, {rq_size}, {pq_size}, {mshr_size}, {hit_latency}, {fill_latency}, {max_read}, {max_write}, {offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, &{lower_level}, {pref_enum_string}, {repl_enum_string});\n'
ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {fill_level}, {pscl5_set}, {pscl5_way}, {pscl4_set}, {pscl4_way}, {pscl3_set}, {pscl3_way}, {pscl2_set}, {pscl2_way}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, 0, {lower_level});\n'

cpu_fmtstr = 'O3_CPU {name}({index}, {frequency}, {DIB[sets]}, {DIB[ways]}, {DIB[window_size]}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{ITLB}, &{DTLB}, &{L1I}, &{L1D}, &{PTW}, {branch_enum_string}, {btb_enum_string});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER {attrs[name]}({attrs[frequency]});\n'
vmem_fmtstr = 'VirtualMemory vmem({attrs[size]}, 1 << 12, {attrs[num_levels]}, 1, {attrs[minor_fault_penalty]});\n'

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
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no_instr', 'replacement': 'lru'}
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': True, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'fill_latency': 1, 'max_read': 2, 'max_write': 2, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': True, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8, 'fill_latency': 1, 'max_read': 1, 'max_write': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 16, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'fill_latency': 1, 'max_read': config_file['num_cores'], 'max_write': config_file['num_cores'], 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru', 'name': 'LLC', 'lower_level': 'DRAM' }
default_pmem = { 'name': 'DRAM', 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'lines_per_column': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'size': 8589934592, 'num_levels': 5, 'minor_fault_penalty': 200 }
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
    cores[i] = ChainMap(cores[i], {'name': 'cpu'+str(i), 'index': i}, copy.deepcopy(dict((k,v) for k,v in config_file.items() if k not in ('ooo_cpu', 'cache'))), default_core.copy())
    cores[i]['DIB'] = ChainMap(cores[i]['DIB'], config_file['DIB'].copy(), default_dib.copy())

# Copy or trim cores as necessary to fill out the specified number of cores
original_size = len(cores)
if original_size <= config_file['num_cores']:
    for i in range(original_size, config_file['num_cores']):
        cores.append(copy.deepcopy(cores[(i-1) % original_size]))
else:
    cores = cores[:(config_file['num_cores'] - original_size)]

# Append LLC to cache array
# LLC operates at maximum freqency of cores, if not already specified
caches['LLC'] = ChainMap(caches.get('LLC',{}), config_file['LLC'].copy(), {'frequency': max(cpu['frequency'] for cpu in cores)}, default_llc.copy())

# If specified in the core, move definition to cache array
for cpu in cores:
    # Assign defaults that are unique per core
    for cache_name in ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB'):
        if isinstance(cpu[cache_name], dict):
            cpu[cache_name] = ChainMap(cpu[cache_name], {'name': cpu['name'] + '_' + cache_name}, config_file[cache_name].copy())
            caches[cpu[cache_name]['name']] = cpu[cache_name]
            cpu[cache_name] = cpu[cache_name]['name']

# Assign defaults that are unique per core
for cpu in cores:
    cpu['PTW'] = ChainMap(cpu.get('PTW',{}), config_file.get('PTW', {}), {'name': cpu['name'] + '_PTW', 'cpu': cpu['index'], 'frequency': cpu['frequency'], 'lower_level': cpu['L1D']}, default_ptw.copy())
    caches[cpu['L1I']] = ChainMap(caches[cpu['L1I']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C'], '_is_instruction_cache': True}, default_l1i.copy())
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

    # LLC
    cache_name = caches[caches[cpu['L1D']]['lower_level']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], default_llc.copy())

# Remove caches that are inaccessible
accessible = [False]*len(caches)
for i,ll in enumerate(caches.values()):
    accessible[i] |= any(ul['lower_level'] == ll['name'] for ul in caches.values()) # The cache is accessible from another cache
    accessible[i] |= any(ll['name'] in [cpu['L1I'], cpu['L1D'], cpu['ITLB'], cpu['DTLB']] for cpu in cores) # The cache is accessible from a core
caches = dict(itertools.compress(caches.items(), accessible))

# Establish latencies in caches
for cache in caches.values():
    cache['hit_latency'] = cache.get('hit_latency') or (cache['latency'] - cache['fill_latency'])

# Create prefetch activation masks
type_list = ('LOAD', 'RFO', 'PREFETCH', 'WRITEBACK', 'TRANSLATION')
for cache in caches.values():
    cache['prefetch_activate_mask'] = functools.reduce(operator.or_, (1 << i for i,t in enumerate(type_list) if t in cache['prefetch_activate'].split(',')))

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

# TLBs use page offsets, Caches use block offsets
for cpu in cores:
    cache_name = cpu['ITLB']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_PAGE_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['DTLB']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_PAGE_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1I']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1D']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        cache_name = caches[cache_name]['lower_level']

# Try the local module directories, then try to interpret as a path
def default_dir(dirname, f):
    fname = os.path.join(dirname, f)
    if not os.path.exists(fname):
        fname = os.path.relpath(os.path.expandvars(os.path.expanduser(f)))
    if not os.path.exists(fname):
        print('Path "' + fname + '" does not exist. Exiting...')
        sys.exit(1)
    return fname

def wrap_list(attr):
    if not isinstance(attr, list):
        attr = [attr]
    return attr

for cache in caches.values():
    cache['replacement'] = [default_dir('replacement', f) for f in wrap_list(cache.get('replacement', []))]
    cache['prefetcher']  = [default_dir('prefetcher', f) for f in wrap_list(cache.get('prefetcher', []))]

for cpu in cores:
    cpu['branch_predictor'] = [default_dir('branch', f) for f in wrap_list(cpu.get('branch_predictor', []))]
    cpu['btb']              = [default_dir('btb', f) for f in wrap_list(cpu.get('btb', []))]

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

def get_module_name(path):
    return path.translate(fname_translation_table)

def get_repl_data(module_name):
    retval = {}

    # Resolve cache replacment function names
    retval['init_func_name'] = 'repl_' + module_name + '_initialize'
    retval['find_victim_func_name'] = 'repl_' + module_name + '_victim'
    retval['update_func_name'] = 'repl_' + module_name + '_update'
    retval['final_func_name'] = 'repl_' + module_name + '_final_stats'

    retval['opts'] = (\
    '-Dinitialize_replacement=' + retval['init_func_name'],\
    '-Dfind_victim=' + retval['find_victim_func_name'],\
    '-Dupdate_replacement_state=' + retval['update_func_name'],\
    '-Dreplacement_final_stats=' + retval['final_func_name']\
    )

    return retval

def get_pref_data(module_name, is_instruction_cache):
    retval = {'_is_instruction_prefetcher': is_instruction_cache}

    prefix = 'ipref_' if is_instruction_cache else 'pref_'
    # Resolve prefetcher function names
    retval['prefetcher_initialize'] = prefix + module_name + '_initialize'
    retval['prefetcher_cache_operate'] = prefix + module_name + '_cache_operate'
    retval['prefetcher_branch_operate'] = prefix + module_name + '_branch_operate'
    retval['prefetcher_cache_fill'] = prefix + module_name + '_cache_fill'
    retval['prefetcher_cycle_operate'] = prefix + module_name + '_cycle_operate'
    retval['prefetcher_final_stats'] = prefix + module_name + '_final_stats'

    retval['opts'] = (\
    # These function names should be used in future designs
    '-Dprefetcher_initialize=' + retval['prefetcher_initialize'],\
    '-Dprefetcher_cache_operate=' + retval['prefetcher_cache_operate'],\
    '-Dprefetcher_branch_operate=' + retval['prefetcher_branch_operate']\
    '-Dprefetcher_cache_fill=' + retval['prefetcher_cache_fill'],\
    '-Dprefetcher_cycle_operate=' + retval['prefetcher_cycle_operate'],\
    '-Dprefetcher_final_stats=' + retval['prefetcher_final_stats'],\
    # These function names are deprecated, but we still permit them
    '-Dl1d_prefetcher_initialize=' + retval['prefetcher_initialize'],\
    '-Dl2c_prefetcher_initialize=' + retval['prefetcher_initialize'],\
    '-Dllc_prefetcher_initialize=' + retval['prefetcher_initialize'],\
    '-Dl1d_prefetcher_operate=' + retval['prefetcher_cache_operate'],\
    '-Dl2c_prefetcher_operate=' + retval['prefetcher_cache_operate'],\
    '-Dllc_prefetcher_operate=' + retval['prefetcher_cache_operate'],\
    '-Dl1d_prefetcher_cache_fill=' + retval['prefetcher_cache_fill'],\
    '-Dl2c_prefetcher_cache_fill=' + retval['prefetcher_cache_fill'],\
    '-Dllc_prefetcher_cache_fill=' + retval['prefetcher_cache_fill'],\
    '-Dl1d_prefetcher_final_stats=' + retval['prefetcher_final_stats'],\
    '-Dl2c_prefetcher_final_stats=' + retval['prefetcher_final_stats'],\
    '-Dllc_prefetcher_final_stats=' + retval['prefetcher_final_stats']\
    )

    return retval

def get_branch_data(module_name):
    retval = {}

    # Resolve branch predictor function names
    retval['bpred_initialize'] = 'bpred_' + module_name + '_initialize'
    retval['bpred_last_result'] = 'bpred_' + module_name + '_last_result'
    retval['bpred_predict'] = 'bpred_' + module_name + '_predict'

    retval['opts'] = (\
    '-Dinitialize_branch_predictor=' + retval['bpred_initialize'],\
    '-Dlast_branch_result=' + retval['bpred_last_result'],\
    '-Dpredict_branch=' + retval['bpred_predict']\
    )

    return retval

def get_btb_data(module_name):
    retval = {}

    # Resolve BTB function names
    retval['btb_initialize'] = 'btb_' + module_name + '_initialize'
    retval['btb_update'] = 'btb_' + module_name + '_update'
    retval['btb_predict'] = 'btb_' + module_name + '_predict'

    retval['opts'] = (\
    '-Dinitialize_btb=' + retval['btb_initialize'],\
    '-Dupdate_btb=' + retval['btb_update'],\
    '-Dbtb_prediction=' + retval['btb_predict']\
    )

    return retval

repl_data   = {get_module_name(fname): {'fname':fname, **get_repl_data(get_module_name(fname))} for fname in itertools.chain.from_iterable(cache['replacement'] for cache in caches.values())}
pref_data   = {get_module_name(fname): {'fname':fname, **get_pref_data(get_module_name(fname),is_instr)} for fname,is_instr in itertools.chain.from_iterable((cache['prefetcher'], cache['_is_instruction_cache']) for cache in caches.values())}
branch_data = {get_module_name(fname): {'fname':fname, **get_branch_data(get_module_name(fname))} for fname in itertools.chain.from_iterable(cpu['branch_predictor'] for cpu in cores)}
btb_data    = {get_module_name(fname): {'fname':fname, **get_btb_data(get_module_name(fname))} for fname in itertools.chain.from_iterable(cpu['btb'] for cpu in cores)}

for cpu in cores:
    cpu['branch_predictor'] = [module_name for module_name,data in branch_data.items() if data['fname'] in cpu['branch_predictor']]
    cpu['btb']              = [module_name for module_name,data in btb_data.items() if data['fname'] in cpu['btb']]

for cache in caches.values():
    cache['replacement'] = [module_name for module_name,data in repl_data.items() if data['fname'] in cache['replacement']]
    cache['prefetcher']  = [module_name for module_name,data in pref_data.items() if data['fname'] in cache['prefetcher']]

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
    wfp.write('#include "ptw.h"\n')
    wfp.write('#include "vmem.h"\n')
    wfp.write('#include "operable.h"\n')
    wfp.write('#include "' + os.path.basename(constants_header_name) + '"\n')
    wfp.write('#include <array>\n')

    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')
    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    for elem in memory_system:
        if 'pscl5_set' in elem:
            wfp.write(ptw_fmtstr.format(**elem))
        else:
            wfp.write(cache_fmtstr.format(\
                repl_enum_string=' | '.join(f'(1 << CACHE::r{k})' for k in elem['replacement']),\
                pref_enum_string=' | '.join(f'(1 << CACHE::p{k})' for k in elem['prefetcher']),\
                **elem))

    for i,cpu in enumerate(cores):
        wfp.write(cpu_fmtstr.format(\
            branch_enum_string=' | '.join(f'(1 << O3_CPU::b{k})' for k in cpu['branch_predictor']),\
            btb_enum_string=' | '.join(f'(1 << O3_CPU::t{k})' for k in cpu['btb']),\
            ipref_enum_string=' | '.join(f'(1 << O3_CPU::i{k})' for k in cpu['iprefetcher']),\
            **cpu))

    wfp.write('std::array<O3_CPU*, NUM_CPUS> ooo_cpu {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in cores))
    wfp.write('\n}};\n')

    wfp.write('std::array<CACHE*, NUM_CACHES> caches {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in memory_system if 'pscl5_set' not in elem))
    wfp.write('\n}};\n')

    wfp.write('std::array<champsim::operable*, NUM_OPERABLES> operables {{\n')
    wfp.write(', '.join('&{name}'.format(**elem) for elem in itertools.chain(cores, memory_system, (config_file['physical_memory'],))))
    wfp.write('\n}};\n')

# Core modules file
with open('inc/ooo_cpu_modules.inc', 'wt') as wfp:
    for i,b in enumerate(branch_data):
        wfp.write(f'constexpr static std::size_t b{b} = {i};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {bpred_initialize}();'.format(**b) for b in branch_data.values()))
    wfp.write('\nvoid impl_branch_predictor_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (bpred_type[b{}]) {bpred_initialize}();'.format(k,**b) for k,b in branch_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {bpred_last_result}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in branch_data.values()))
    wfp.write('\nvoid impl_last_branch_result(uint64_t ip, uint64_t target, uint8_t taken, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (bpred_type[b{}]) {bpred_last_result}(ip, target, taken, branch_type);'.format(k,**b) for k,b in branch_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint8_t {bpred_predict}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in branch_data.values()))
    wfp.write('\nuint8_t impl_predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)\n{\n    ')
    wfp.write('std::bitset<NUM_BRANCH_MODULES> result;\n    ')
    wfp.write('\n    '.join('if (bpred_type[b{0}]) result[b{0}] = {bpred_predict}(ip, predicted_target, always_taken, branch_type);'.format(k,**b) for k,b in branch_data.items()))
    wfp.write('\n    return result.any();')
    wfp.write('\n}\n\n')

    for i,b in enumerate(btb_data):
        wfp.write(f'constexpr static int t{b} = {i};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {btb_initialize}();'.format(**b) for b in btb_data.values()))
    wfp.write('\nvoid impl_btb_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (btb_type[t{}]) {btb_initialize}();'.format(k,**b) for k,b in btb_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {btb_update}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in btb_data.values()))
    wfp.write('\nvoid impl_update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (btb_type[t{}]) {btb_update}(ip, branch_target, taken, branch_type);'.format(k,**b) for k,b in btb_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('std::pair<uint64_t, uint8_t> {btb_predict}(uint64_t, uint8_t);'.format(**b) for b in btb_data.values()))
    wfp.write('\nstd::pair<uint64_t, uint8_t> impl_btb_prediction(uint64_t ip, uint8_t branch_type)\n{\n    ')
    wfp.write('std::pair<uint64_t, uint8_t> result;\n    ')
    wfp.write('\n    '.join('if (btb_type[t{}]) result = {btb_predict}(ip, branch_type);'.format(k,**b) for k,b in btb_data.items()))
    wfp.write('\n    return result;')
    wfp.write('\n}\n')
    wfp.write('\n')

with open('inc/cache_modules.inc', 'wt') as wfp:
    for i,b in enumerate(repl_data):
        wfp.write(f'constexpr static int r{b} = {i};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {init_func_name}();'.format(**r) for r in repl_data.values()))
    wfp.write('\nvoid impl_replacement_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (repl_type[r{}]) {init_func_name}();'.format(k,**v) for k,v in repl_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint32_t {find_victim_func_name}(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t);'.format(**r) for r in repl_data.values()))
    wfp.write('\nuint32_t impl_replacement_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)\n{\n    ')
    wfp.write('uint32_t result = NUM_WAY;\n    ')
    wfp.write('\n    '.join('if (repl_type[r{}]) result = {find_victim_func_name}(cpu, instr_id, set, current_set, ip, full_addr, type);'.format(k,**v) for k,v in repl_data.items()))
    wfp.write('\n    return result;')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {update_func_name}(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t);'.format(**r) for r in repl_data.values()))
    wfp.write('\nvoid impl_replacement_update_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)\n{\n    ')
    wfp.write('\n    '.join('if (repl_type[r{}]) {update_func_name}(cpu, set, way, full_addr, ip, victim_addr, type, hit);'.format(k,**v) for k,v in repl_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {final_func_name}();'.format(**r) for r in repl_data.values()))
    wfp.write('\nvoid impl_replacement_final_stats()\n{\n    ')
    wfp.write('\n    '.join('if (repl_type[r{}]) {final_func_name}();'.format(k,**v) for k,v in repl_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    for i,b in enumerate(pref_data):
        wfp.write(f'constexpr static std::size_t p{b} = {i};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {prefetcher_initialize}();'.format(**p) for p in pref_data.values()))
    wfp.write('\nvoid impl_prefetcher_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type[p{}]) {prefetcher_initialize}();'.format(k,**p) for k,p in pref_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')
    
    wfp.write('\n'.join('void {prefetcher_branch_operate}(uint64_t, uint8_t, uint64_t);\n'.format(**p) for p in pref_data.values() if '_is_instruction_prefetcher' in p))
    wfp.write('\n'.join('void {prefetcher_branch_operate}(uint64_t, uint8_t, uint64_t) {{ assert(false); }}\n'.format(**p) for p in pref_data.values() if '_is_instruction_prefetcher' not in p))
    wfp.write('\nvoid impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}(ip, branch_type, branch_target);'.format(*i) for i in pref_branch))
    wfp.write('\n    throw std::invalid_argument("Instruction prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')
    
    wfp.write('\n'.join('uint32_t {prefetcher_cache_operate}(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t);'.format(**p) for p in pref_data.values() if not p['prefetcher_cache_operate'].startswith('ooo_cpu')))
    wfp.write('\nuint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)\n{\n    ')
    wfp.write('uint32_t result = 0;\n')
    wfp.write('\n    '.join('if (pref_type[p{}]) result ^= {prefetcher_cache_operate}(addr, ip, cache_hit, type, metadata_in);\n'.format(k,**p) for k,p in pref_data.items()))
    wfp.write('    return result;')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint32_t {prefetcher_cache_fill}(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);'.format(**p) for p in pref_data.values() if not p['prefetcher_cache_fill'].startswith('ooo_cpu')))
    wfp.write('\nuint32_t impl_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)\n{\n    ')
    wfp.write('uint32_t result = 0;\n    ')
    wfp.write('\n    '.join('if (pref_type[p{}]) result ^= {prefetcher_cache_fill}(addr, set, way, prefetch, evicted_addr, metadata_in);'.format(k,**p) for k,p in pref_data.items()))
    wfp.write('\n    return result;')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {prefetcher_cycle_operate}();'.format(**p) for p in pref_data.values() if not p['prefetcher_cycle_operate'].startswith('ooo_cpu')))
    wfp.write('\nvoid impl_prefetcher_cycle_operate()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type[p{}]) {prefetcher_cycle_operate}();'.format(k,**p) for k,p in pref_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {prefetcher_final_stats}();'.format(**p) for p in pref_data.values() if not p['prefetcher_final_stats'].startswith('ooo_cpu')))
    wfp.write('\nvoid impl_prefetcher_final_stats()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type[p{}]) {prefetcher_final_stats}();'.format(k,**p) for k,p in pref_data.items()))
    wfp.write('\n}\n')
    wfp.write('\n')

pmem_const_names = {
    # config_file_key : (Type, Needs LOG2?, NAME)
    'io_freq': ('unsigned long', False, 'DRAM_IO_FREQ'),
    'channels': ('unsigned long', True, 'DRAM_CHANNELS'),
    'ranks': ('unsigned long', True, 'DRAM_RANKS'),
    'banks': ('unsigned long', True, 'DRAM_BANKS'),
    'rows': ('unsigned long', True, 'DRAM_ROWS'),
    'columns': ('unsigned long', True, 'DRAM_COLUMNS'),
    'row_size': ('unsigned long', False, 'DRAM_ROW_SIZE'),
    'channel_width': ('unsigned long', False, 'DRAM_CHANNEL_WIDTH'),
    'wq_size': ('std::size_t', False, 'DRAM_WQ_SIZE'),
    'rq_size': ('std::size_t', False, 'DRAM_RQ_SIZE'),
    'tRP': ('double', False, 'tRP_DRAM_NANOSECONDS'),
    'tRCD': ('double', False, 'tRCD_DRAM_NANOSECONDS'),
    'tCAS': ('double', False, 'tCAS_DRAM_NANOSECONDS'),
    'turn_around_time': ('double', False, 'DBUS_TURN_AROUND_NANOSECONDS')
}

# Constants header
with open(constants_header_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#ifndef CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#define CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#include <cstdlib>\n')
    wfp.write('#include "util.h"\n')
    wfp.write(f"constexpr static unsigned long BLOCK_SIZE = {config_file['block_size']};\n")
    wfp.write('constexpr static auto LOG2_BLOCK_SIZE = lg2(BLOCK_SIZE);\n')
    wfp.write(f"constexpr static unsigned long PAGE_SIZE = {config_file['page_size']};\n")
    wfp.write('constexpr static auto LOG2_PAGE_SIZE = lg2(PAGE_SIZE);\n')
    wfp.write(f"constexpr static unsigned long STAT_PRINTING_PERIOD = {config_file['heartbeat_frequency']};\n")
    wfp.write(f'constexpr static std::size_t NUM_CPUS = {len(cores)};\n')
    wfp.write(f'constexpr static std::size_t NUM_CACHES = {len(caches)};\n')
    wfp.write(f'constexpr static std::size_t NUM_OPERABLES = {len(cores) + len(memory_system) + 1};\n')
    wfp.write(f'constexpr static std::size_t NUM_BRANCH_MODULES = {len(branch_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_BTB_MODULES = {len(btb_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_IPREFETCH_MODULES = {len(ipref_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_REPLACEMENT_MODULES = {len(repl_data)};\n')
    wfp.write(f'constexpr static std::size_t NUM_PREFETCH_MODULES = {len(pref_data)};\n')

    for k,v in pmem_const_names.items():
        t, l, n = v
        value = config_file['physical_memory'][k]
        wfp.write(f"constexpr static {t} {n} = {value};\n")
        if l:
            wfp.write(f'constexpr static auto LOG2_{n} = lg2({n});\n')

    wfp.write('#endif\n')

# Makefile
module_file_info = tuple( (v['fname'], v['opts']) for k,v in itertools.chain(repl_data.items(), pref_data.items(), branch_data.items(), btb_data.items()) ) # Associate modules with paths
with open('Makefile', 'wt') as wfp:
    wfp.write('CC := ' + config_file.get('CC', 'gcc') + '\n')
    wfp.write('CXX := ' + config_file.get('CXX', 'g++') + '\n')
    #wfp.write('CFLAGS := ' + config_file.get('CFLAGS', '-Wall -O3') + ' -std=gnu99\n')
    wfp.write('CXXFLAGS := ' + config_file.get('CXXFLAGS', '-Wall -O3') + ' -std=c++17\n')
    wfp.write('CPPFLAGS := ' + config_file.get('CPPFLAGS', '') + ' -Iinc -MMD -MP\n')
    wfp.write('LDFLAGS := ' + config_file.get('LDFLAGS', '') + '\n')
    wfp.write('LDLIBS := ' + config_file.get('LDLIBS', '') + '\n')
    wfp.write('\n')
    wfp.write('.phony: all clean\n\n')

    wfp.write('executable_name ?= ' + config_file['executable_name'] + '\n\n')
    wfp.write('all: $(executable_name)\n\n')
    wfp.write('clean: \n')
    wfp.write('\t$(RM) ' + constants_header_name + '\n')
    wfp.write('\t$(RM) ' + instantiation_file_name + '\n')
    wfp.write('\t$(RM) ' + 'inc/cache_modules.inc' + '\n')
    wfp.write('\t$(RM) ' + 'inc/ooo_cpu_modules.inc' + '\n')
    wfp.write('\t find . -name \*.o -delete\n\t find . -name \*.d -delete\n\t $(RM) -r obj\n\n')
    for topdir, _ in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
        for d in dir:
            wfp.write(f'\t find {d} -name \*.o -delete\n\t find {d} -name \*.d -delete\n')
    wfp.write('\n')

    wfp.write('cxxsources = $(wildcard src/*.cc)\n')
    wfp.write('\n')

    for topdir, opts in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
            for f in files:
                if os.path.splitext(f)[1] in ['.cc', '.cpp', '.C']:
                    name = os.path.join(b,f)
                    wfp.write(f'cxxsources += {name}\n')
        for d in dir:
            wfp.write(f'{d}/%.o: CPPFLAGS += -I{d}\n')
            wfp.write('\n'.join(f'{d}/%.o: CXXFLAGS += {opt}' for opt in opts))
        wfp.write('\n')
        wfp.write('\n')

    wfp.write('$(executable_name): $(patsubst %.cc,%.o,$(cxxsources))\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    wfp.write('-include $(wildcard src/*.d)\n')
    for topdir, _ in module_file_info:
        dir = [topdir]
        for b,dd,files in os.walk(topdir):
            dir.extend(os.path.join(b,d) for d in dd)
        for d in dir:
            wfp.write(f'-include $(wildcard {d}/*.d)\n')
    wfp.write('\n')

