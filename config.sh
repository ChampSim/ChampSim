#!/usr/bin/env python3
import json
import sys,os
import itertools
import functools
import operator
import copy
from collections import ChainMap

import config.makefile as makefile

constants_header_name = 'inc/champsim_constants.h'
instantiation_file_name = 'src/core_inst.cc'

fname_translation_table = str.maketrans('./-','_DH')

def norm_fname(fname):
    return os.path.relpath(os.path.expandvars(os.path.expanduser(fname)))

###
# Begin format strings
###

ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {pscl5_set}, {pscl5_way}, {pscl4_set}, {pscl4_way}, {pscl3_set}, {pscl3_way}, {pscl2_set}, {pscl2_way}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, 0, {lower_level});\n'

cpu_fmtstr = 'O3_CPU {name}({index}, {frequency}, {DIB[sets]}, {DIB[ways]}, {DIB[window_size]}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{L1I}, &{L1D}, O3_CPU::bpred_t::{bpred_name}, O3_CPU::btb_t::{btb_name});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER {attrs[name]}({attrs[frequency]});\n'
vmem_fmtstr = 'VirtualMemory vmem({attrs[size]}, 1 << 12, {attrs[num_levels]}, 1, {attrs[minor_fault_penalty]});\n'

###
# Begin default core model definition
###

default_root = { 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1, 'DIB': {}, 'L1I': {}, 'L1D': {}, 'L2C': {}, 'ITLB': {}, 'DTLB': {}, 'STLB': {}, 'LLC': {}, 'physical_memory': {}, 'virtual_memory': {}}

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
    caches[cpu['L1I']] = ChainMap(caches[cpu['L1I']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C'], 'lower_translate': cpu['ITLB'], '_needs_translate': True, '_is_instruction_cache': True}, default_l1i.copy())
    caches[cpu['L1D']] = ChainMap(caches[cpu['L1D']], {'frequency': cpu['frequency'], 'lower_level': cpu['L2C'], 'lower_translate': cpu['DTLB'], '_needs_translate': True}, default_l1d.copy())
    caches[cpu['ITLB']] = ChainMap(caches[cpu['ITLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_itlb.copy())
    caches[cpu['DTLB']] = ChainMap(caches[cpu['DTLB']], {'frequency': cpu['frequency'], 'lower_level': cpu['STLB']}, default_dtlb.copy())

    # L2C
    cache_name = caches[cpu['L1D']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': 'LLC', 'lower_translate': caches[cpu['DTLB']]['lower_level']}, default_l2c.copy())

    # STLB
    cache_name = caches[cpu['DTLB']]['lower_level']
    if cache_name != 'DRAM':
        caches[cache_name] = ChainMap(caches[cache_name], {'frequency': cpu['frequency'], 'lower_level': cpu['PTW']['name']}, default_stlb.copy())

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
        caches[cache_name]['_needs_translate'] = False
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['DTLB']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_PAGE_SIZE'
        caches[cache_name]['_needs_translate'] = False
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1I']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        caches[cache_name]['_needs_translate'] = caches[cache_name].get('_needs_translate', False) or caches[cache_name].get('virtual_prefetch', False)
        cache_name = caches[cache_name]['lower_level']

    cache_name = cpu['L1D']
    while cache_name in caches:
        caches[cache_name]['offset_bits'] = 'LOG2_BLOCK_SIZE'
        caches[cache_name]['_needs_translate'] = caches[cache_name].get('_needs_translate', False) or caches[cache_name].get('virtual_prefetch', False)
        cache_name = caches[cache_name]['lower_level']

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

# Associate modules with paths
libfilenames = {}

for cache in caches.values():
    # Resolve cache replacment function names
    if cache['replacement'] is not None:
        fname = os.path.join('replacement', cache['replacement'])
        if not os.path.exists(fname):
            fname = norm_fname(cache['replacement'])
        if not os.path.exists(fname):
            print('Path "' + fname + '" does not exist. Exiting...')
            sys.exit(1)

        cache['replacement_name'] = 'r' + fname.translate(fname_translation_table)
        cache['replacement_initialize'] = 'repl_' + cache['replacement_name'] + '_initialize'
        cache['replacement_find_victim'] = 'repl_' + cache['replacement_name'] + '_victim'
        cache['replacement_update_replacement_state'] = 'repl_' + cache['replacement_name'] + '_update'
        cache['replacement_replacement_final_stats'] = 'repl_' + cache['replacement_name'] + '_final_stats'

        opts = ''
        opts += ' -Dinitialize_replacement=' + cache['replacement_initialize']
        opts += ' -Dfind_victim=' + cache['replacement_find_victim']
        opts += ' -Dupdate_replacement_state=' + cache['replacement_update_replacement_state']
        opts += ' -Dreplacement_final_stats=' + cache['replacement_replacement_final_stats']
        libfilenames['repl_' + cache['replacement_name'] + '.a'] = (fname, opts)

    # Resolve prefetcher function names
    if cache['prefetcher'] is not None:
        fname = os.path.join('prefetcher', cache['prefetcher'])
        if not os.path.exists(fname):
            fname = norm_fname(cache['prefetcher'])
        if not os.path.exists(fname):
            print('Path "' + fname + '" does not exist. Exiting...')
            sys.exit(1)

        cache['prefetcher_name'] = 'p' + fname.translate(fname_translation_table)

        prefix = 'ipref_' if cache.get('_is_instruction_cache') else 'pref_'
        cache['prefetcher_initialize'] = prefix + cache['prefetcher_name'] + '_initialize'
        cache['prefetcher_branch_operate'] = prefix + cache['prefetcher_name'] + '_branch_operate'
        cache['prefetcher_cache_operate'] = prefix + cache['prefetcher_name'] + '_cache_operate'
        cache['prefetcher_cache_fill'] = prefix + cache['prefetcher_name'] + '_cache_fill'
        cache['prefetcher_cycle_operate'] = prefix + cache['prefetcher_name'] + '_cycle_operate'
        cache['prefetcher_final_stats'] = prefix + cache['prefetcher_name'] + '_final_stats'

        opts = ''
        # These function names should be used in future designs
        opts += ' -Dprefetcher_initialize=' + cache['prefetcher_initialize']
        opts += ' -Dprefetcher_branch_operate=' + cache['prefetcher_branch_operate']
        opts += ' -Dprefetcher_cache_operate=' + cache['prefetcher_cache_operate']
        opts += ' -Dprefetcher_cache_fill=' + cache['prefetcher_cache_fill']
        opts += ' -Dprefetcher_cycle_operate=' + cache['prefetcher_cycle_operate']
        opts += ' -Dprefetcher_final_stats=' + cache['prefetcher_final_stats']
        # These function names are deprecated, but we still permit them
        opts += ' -Dl1i_prefetcher_branch_operate=' + cache['prefetcher_branch_operate']
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
        libfilenames['pref_' + cache['prefetcher_name'] + '.a'] = (fname, opts)

for cpu in cores:
    # Resolve branch predictor function names
    if cpu['branch_predictor'] is not None:
        fname = os.path.join('branch', cpu['branch_predictor'])
        if not os.path.exists(fname):
            fname = norm_fname(cpu['branch_predictor'])
        if not os.path.exists(fname):
            print('Path "' + fname + '" does not exist. Exiting...')
            sys.exit(1)

        cpu['bpred_name'] = 'b' + fname.translate(fname_translation_table)
        cpu['bpred_initialize'] = 'bpred_' + cpu['bpred_name'] + '_initialize'
        cpu['bpred_last_result'] = 'bpred_' + cpu['bpred_name'] + '_last_result'
        cpu['bpred_predict'] = 'bpred_' + cpu['bpred_name'] + '_predict'

        opts = ''
        opts += ' -Dinitialize_branch_predictor=' + cpu['bpred_initialize']
        opts += ' -Dlast_branch_result=' + cpu['bpred_last_result']
        opts += ' -Dpredict_branch=' + cpu['bpred_predict']
        libfilenames['bpred_' + cpu['bpred_name'] + '.a'] = (fname, opts)

    # Resolve BTB function names
    if cpu['btb'] is not None:
        fname = os.path.join('btb', cpu['btb'])
        if not os.path.exists(fname):
            fname = norm_fname(cpu['btb'])
        if not os.path.exists(fname):
            print('Path "' + fname + '" does not exist. Exiting...')
            sys.exit(1)

        cpu['btb_name'] = 'b' + fname.translate(fname_translation_table)
        cpu['btb_initialize'] = 'btb_' + cpu['btb_name'] + '_initialize'
        cpu['btb_update'] = 'btb_' + cpu['btb_name'] + '_update'
        cpu['btb_predict'] = 'btb_' + cpu['btb_name'] + '_predict'

        opts = ''
        opts += ' -Dinitialize_btb=' + cpu['btb_initialize']
        opts += ' -Dupdate_btb=' + cpu['btb_update']
        opts += ' -Dbtb_prediction=' + cpu['btb_predict']
        libfilenames['btb_' + cpu['btb_name'] + '.a'] = (fname, opts)

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
for fill_level in range(1,len(memory_system)+1):
    for k in active_keys:
        memory_system[k]['_fill_level'] = fill_level
    active_keys = [memory_system[k]['lower_level'] for k in active_keys if memory_system[k]['lower_level'] != 'DRAM']

# Remove name index
memory_system = list(memory_system.values())

memory_system.sort(key=operator.itemgetter('_fill_level'), reverse=True)

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
cache_fmtstr = 'CACHE {name}{{"{name}", {frequency}, {sets}, {ways}, {mshr_size}, {fill_latency}, {max_read}, {max_write}, {offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, {prefetch_activate_mask}, {name}_queues, {lower_level}, CACHE::pref_t::{prefetcher_name}, CACHE::repl_t::{replacement_name}}};\n'
transl_queue_fmtstr = 'CACHE::TranslatingQueues {name}_queues{{{frequency}, {rq_size}, {pq_size}, {wq_size}, {hit_latency}, {offset_bits}, {wq_check_full_addr:b}}};\n'
nontransl_queue_fmtstr = 'CACHE::NonTranslatingQueues {name}_queues{{{frequency}, {rq_size}, {pq_size}, {wq_size}, {hit_latency}, {offset_bits}, {wq_check_full_addr:b}}};\n'
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
    wfp.write('#include <functional>\n')
    wfp.write('#include <vector>\n')

    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')
    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    for elem in memory_system:
        if 'pscl5_set' in elem:
            wfp.write(ptw_fmtstr.format(**elem))
        else:
            if elem.get('_needs_translate'):
                wfp.write(transl_queue_fmtstr.format(**elem))
            else:
                wfp.write(nontransl_queue_fmtstr.format(**elem))

            wfp.write(cache_fmtstr.format(**elem))

    for cpu in cores:
        wfp.write(cpu_fmtstr.format(**cpu))

    wfp.write('std::array<std::reference_wrapper<O3_CPU>, NUM_CPUS> ooo_cpu {{\n')
    wfp.write(', '.join('{name}'.format(**elem) for elem in cores))
    wfp.write('\n}};\n')

    wfp.write('std::array<std::reference_wrapper<CACHE>, NUM_CACHES> caches {{\n')
    wfp.write(', '.join('{name}'.format(**elem) for elem in reversed(memory_system) if 'pscl5_set' not in elem))
    wfp.write('\n}};\n')

    wfp.write('std::array<std::reference_wrapper<champsim::operable>, 2*NUM_CPUS + 2*NUM_CACHES + 1> operables {{\n')
    wfp.write(', '.join('{name}'.format(**elem) for elem in cores))
    wfp.write(',\n')
    wfp.write(', '.join('{name}'.format(**elem) for elem in memory_system if 'pscl5_set' in elem))
    wfp.write(',\n')
    wfp.write(', '.join('{name}, {name}_queues'.format(**elem) for elem in memory_system if 'pscl5_set' not in elem))
    wfp.write(',\n')
    wfp.write('{name}'.format(**config_file['physical_memory']))
    wfp.write('\n}};\n')

    wfp.write('\n')
    wfp.write('void init_structures() {\n')
    for elem in memory_system:
        if elem.get('_needs_translate'):
            wfp.write('  {name}_queues.lower_level = &{lower_translate};\n'.format(**elem))
    wfp.write('}\n')

# Core modules file
bpred_names        = {c['bpred_name'] for c in cores}
bpred_inits        = {(c['bpred_name'], c['bpred_initialize']) for c in cores}
bpred_last_results = {(c['bpred_name'], c['bpred_last_result']) for c in cores}
bpred_predicts     = {(c['bpred_name'], c['bpred_predict']) for c in cores}
btb_names          = {c['btb_name'] for c in cores}
btb_inits          = {(c['btb_name'], c['btb_initialize']) for c in cores}
btb_updates        = {(c['btb_name'], c['btb_update']) for c in cores}
btb_predicts       = {(c['btb_name'], c['btb_predict']) for c in cores}
with open('inc/ooo_cpu_modules.inc', 'wt') as wfp:
    wfp.write('enum class bpred_t\n{\n    ')
    wfp.write(',\n    '.join(bpred_names))
    wfp.write('\n};\n\n')

    wfp.write('\n'.join('void {1}();'.format(*b) for b in bpred_inits))
    wfp.write('\nvoid impl_branch_predictor_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (bpred_type == bpred_t::{}) return {}();'.format(*b) for b in bpred_inits))
    wfp.write('\n    throw std::invalid_argument("Branch predictor module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(*b) for b in bpred_last_results))
    wfp.write('\nvoid impl_last_branch_result(uint64_t ip, uint64_t target, uint8_t taken, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (bpred_type == bpred_t::{}) return {}(ip, target, taken, branch_type);'.format(*b) for b in bpred_last_results))
    wfp.write('\n    throw std::invalid_argument("Branch predictor module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint8_t {1}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(*b) for b in bpred_predicts))
    wfp.write('\nuint8_t impl_predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (bpred_type == bpred_t::{}) return {}(ip, predicted_target, always_taken, branch_type);'.format(*b) for b in bpred_predicts))
    wfp.write('\n    throw std::invalid_argument("Branch predictor module not found");')
    wfp.write('\n    return 0;\n}\n\n')

    wfp.write('enum class btb_t\n{\n    ')
    wfp.write(',\n    '.join(btb_names))
    wfp.write('\n};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*b) for b in btb_inits))
    wfp.write('\nvoid impl_btb_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (btb_type == btb_t::{}) return {}();'.format(*b) for b in btb_inits))
    wfp.write('\n    throw std::invalid_argument("Branch target buffer module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(*b) for b in btb_updates))
    wfp.write('\nvoid impl_update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (btb_type == btb_t::{}) return {}(ip, branch_target, taken, branch_type);'.format(*b) for b in btb_updates))
    wfp.write('\n    throw std::invalid_argument("Branch target buffer module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('std::pair<uint64_t, uint8_t> {1}(uint64_t, uint8_t);'.format(*b) for b in btb_predicts))
    wfp.write('\nstd::pair<uint64_t, uint8_t> impl_btb_prediction(uint64_t ip, uint8_t branch_type)\n{\n    ')
    wfp.write('\n    '.join('if (btb_type == btb_t::{}) return {}(ip, branch_type);'.format(*b) for b in btb_predicts))
    wfp.write('\n    throw std::invalid_argument("Branch target buffer module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

# Cache modules file
repl_names   = {c['replacement_name'] for c in caches.values()}
repl_inits   = {(c['replacement_name'], c['replacement_initialize']) for c in caches.values()}
repl_victims = {(c['replacement_name'], c['replacement_find_victim']) for c in caches.values()}
repl_updates = {(c['replacement_name'], c['replacement_update_replacement_state']) for c in caches.values()}
repl_finals  = {(c['replacement_name'], c['replacement_replacement_final_stats']) for c in caches.values()}
pref_names   = {c['prefetcher_name'] for c in caches.values()}
pref_inits   = {(c['prefetcher_name'], c['prefetcher_initialize']) for c in caches.values()}
pref_branch  = {(c['prefetcher_name'], c['prefetcher_branch_operate'], c.get('_is_instruction_cache')) for c in caches.values()}
pref_ops     = {(c['prefetcher_name'], c['prefetcher_cache_operate']) for c in caches.values()}
pref_fill    = {(c['prefetcher_name'], c['prefetcher_cache_fill']) for c in caches.values()}
pref_cycles  = {(c['prefetcher_name'], c['prefetcher_cycle_operate']) for c in caches.values()}
pref_finals  = {(c['prefetcher_name'], c['prefetcher_final_stats']) for c in caches.values()}
with open('inc/cache_modules.inc', 'wt') as wfp:
    wfp.write('enum class repl_t\n{\n    ')
    wfp.write(',\n    '.join(repl_names))
    wfp.write('\n};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*r) for r in repl_inits))
    wfp.write('\nvoid impl_replacement_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (repl_type == repl_t::{}) return {}();'.format(*r) for r in repl_inits))
    wfp.write('\n    throw std::invalid_argument("Replacement policy module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint32_t {1}(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t);'.format(*r) for r in repl_victims))
    wfp.write('\nuint32_t impl_replacement_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)\n{\n    ')
    wfp.write('\n    '.join('if (repl_type == repl_t::{}) return {}(cpu, instr_id, set, current_set, ip, full_addr, type);'.format(*r) for r in repl_victims))
    wfp.write('\n    throw std::invalid_argument("Replacement policy module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t);'.format(*r) for r in repl_updates))
    wfp.write('\nvoid impl_replacement_update_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)\n{\n    ')
    wfp.write('\n    '.join('if (repl_type == repl_t::{}) return {}(cpu, set, way, full_addr, ip, victim_addr, type, hit);'.format(*r) for r in repl_updates))
    wfp.write('\n    throw std::invalid_argument("Replacement policy module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*r) for r in repl_finals))
    wfp.write('\nvoid impl_replacement_final_stats()\n{\n    ')
    wfp.write('\n    '.join('if (repl_type == repl_t::{}) return {}();'.format(*r) for r in repl_finals))
    wfp.write('\n    throw std::invalid_argument("Replacement policy module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('enum class pref_t\n{\n    ')
    wfp.write(',\n    '.join(pref_names))
    wfp.write('\n};\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*p) for p in pref_inits))
    wfp.write('\nvoid impl_prefetcher_initialize()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}();'.format(*p) for p in pref_inits))
    wfp.write('\n    throw std::invalid_argument("Data prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    for n,f,is_instr in pref_branch:
        if is_instr:
            wfp.write('void {}(uint64_t, uint8_t, uint64_t);\n'.format(f))
        else:
            wfp.write('void {}(uint64_t, uint8_t, uint64_t) {{ assert(false); }}\n'.format(f))
    wfp.write('\nvoid impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}(ip, branch_type, branch_target);'.format(*i) for i in pref_branch))
    wfp.write('\n    throw std::invalid_argument("Instruction prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint32_t {1}(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t);'.format(*p) for p in pref_ops))
    wfp.write('\nuint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}(addr, ip, cache_hit, type, metadata_in);'.format(*p) for p in pref_ops))
    wfp.write('\n    throw std::invalid_argument("Data prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('uint32_t {1}(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);'.format(*p) for p in pref_fill))
    wfp.write('\nuint32_t impl_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}(addr, set, way, prefetch, evicted_addr, metadata_in);'.format(*p) for p in pref_fill))
    wfp.write('\n    throw std::invalid_argument("Data prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*p) for p in pref_cycles))
    wfp.write('\nvoid impl_prefetcher_cycle_operate()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}();'.format(*p) for p in pref_cycles))
    wfp.write('\n    throw std::invalid_argument("Data prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

    wfp.write('\n'.join('void {1}();'.format(*p) for p in pref_finals))
    wfp.write('\nvoid impl_prefetcher_final_stats()\n{\n    ')
    wfp.write('\n    '.join('if (pref_type == pref_t::{}) return {}();'.format(*p) for p in pref_finals))
    wfp.write('\n    throw std::invalid_argument("Data prefetcher module not found");')
    wfp.write('\n}\n')
    wfp.write('\n')

# Constants header
with open(constants_header_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#ifndef CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#define CHAMPSIM_CONSTANTS_H\n')
    wfp.write('#include "util.h"\n')
    wfp.write('constexpr unsigned BLOCK_SIZE = {block_size};\n'.format(**config_file))
    wfp.write('constexpr unsigned PAGE_SIZE = {page_size};\n'.format(**config_file))
    wfp.write('constexpr uint64_t STAT_PRINTING_PERIOD = {heartbeat_frequency};\n'.format(**config_file))
    wfp.write('constexpr std::size_t NUM_CPUS = {num_cores};\n'.format(**config_file))
    wfp.write('constexpr std::size_t NUM_CACHES = ' + str(len(caches)) + ';\n')
    wfp.write('constexpr auto LOG2_BLOCK_SIZE = lg2(BLOCK_SIZE);\n')
    wfp.write('constexpr auto LOG2_PAGE_SIZE = lg2(PAGE_SIZE);\n')

    wfp.write('constexpr uint64_t DRAM_IO_FREQ = {io_freq};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_CHANNELS = {channels};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_RANKS = {ranks};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_BANKS = {banks};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_ROWS = {rows};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_COLUMNS = {columns};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_CHANNEL_WIDTH = {channel_width};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_WQ_SIZE = {wq_size};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr std::size_t DRAM_RQ_SIZE = {rq_size};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr double tRP_DRAM_NANOSECONDS = {tRP};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr double tRCD_DRAM_NANOSECONDS = {tRCD};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr double tCAS_DRAM_NANOSECONDS = {tCAS};\n'.format(**config_file['physical_memory']))
    wfp.write('constexpr double DBUS_TURN_AROUND_NANOSECONDS = {turn_around_time};\n'.format(**config_file['physical_memory']))

    wfp.write('#endif\n')

# Makefile
with open('_configuration.mk', 'wt') as wfp:
    wfp.write(makefile.get_makefile_string(constants_header_name, instantiation_file_name, libfilenames, **config_file))

