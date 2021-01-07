#!/usr/bin/env python3
import json
import sys,os
import itertools
import functools

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

llc_fmtstr = 'CACHE {name}("{name}", {attrs[sets]}, {attrs[ways]}, {attrs[wq_size]}, {attrs[rq_size]}, {attrs[pq_size]}, {attrs[mshr_size]}, {attrs[max_read]}, {attrs[max_write]});\n'

cpu_fmtstr = 'O3_CPU cpu{cpu}_inst({cpu}, {attrs[DIB][sets]}, {attrs[DIB][ways]}, {attrs[DIB][window_size]}, {attrs[ifetch_buffer_size]}, {attrs[dispatch_buffer_size]}, {attrs[decode_buffer_size]}, {attrs[rob_size]}, {attrs[lq_size]}, {attrs[sq_size]}, {attrs[fetch_width]}, {attrs[decode_width]}, {attrs[dispatch_width]}, {attrs[scheduler_size]}, {attrs[execute_width]}, {attrs[lq_width]}, {attrs[sq_width]}, {attrs[retire_width]}, {attrs[mispredict_penalty]}, {attrs[decode_latency]}, {attrs[dispatch_latency]}, {attrs[schedule_latency]}, {attrs[execute_latency]});\n'

pmem_fmtstr = 'MEMORY_CONTROLLER DRAM("DRAM");\n'
vmem_fmtstr = 'VirtualMemory vmem(NUM_CPUS, {attrs[size]}, PAGE_SIZE, {attrs[num_levels]}, 1);\n'

module_make_fmtstr = 'obj/{}: $(patsubst %.cc,%.o,$(wildcard {}/*.cc))\n\t@mkdir -p $(dir $@)\n\tar -rcs $@ $^\n\n'

define_fmtstr = '#define {{names[{name}]}} {{config[{name}]}}u\n'
define_nonint_fmtstr = '#define {{names[{name}]}} {{config[{name}]}}\n'
define_log_fmtstr = '#define LOG2_{{names[{name}]}} lg2({{names[{name}]}})\n'
cache_define_fmtstr = '#define {name}_SET {attrs[sets]}u\n#define {name}_WAY {attrs[ways]}u\n#define {name}_WQ_SIZE {attrs[wq_size]}u\n#define {name}_RQ_SIZE {attrs[rq_size]}u\n#define {name}_PQ_SIZE {attrs[pq_size]}u\n#define {name}_MSHR_SIZE {attrs[mshr_size]}u\n#define {name}_LATENCY {attrs[latency]}u\n#define {name}_MAX_READ {attrs[max_read]}\n#define {name}_MAX_WRITE {attrs[max_write]}\n'

###
# Begin named constants
###

const_names = {
    'block_size': 'BLOCK_SIZE',
    'page_size': 'PAGE_SIZE',
    'heartbeat_frequency': 'STAT_PRINTING_PERIOD',
    'cpu_clock_freq': 'CPU_FREQ',
    'num_cores': 'NUM_CPUS',
    'physical_memory': {
        'frequency': 'DRAM_IO_FREQ',
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
        'tCAS': 'tCAS_DRAM_NANOSECONDS'
    }
}

###
# Begin default core model definition
###

default_root = { 'executable_name': 'bin/champsim', 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'cpu_clock_freq' : 4000, 'num_cores': 1, 'ooo_cpu': [{}] }
config_file = merge_dicts(default_root, config_file) # doing this early because LLC dimensions depend on it

default_core = { 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'max_read': 2, 'max_write': 2, 'prefetcher': 'no_l1i' }
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'max_read': 2, 'max_write': 2, 'prefetcher': 'no_l1d' }
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'max_read': 1, 'max_write': 1, 'prefetcher': 'no_l2c' }
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'max_read': 2, 'max_write': 2 }
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1, 'max_read': 2, 'max_write': 2 }
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8, 'max_read': 1, 'max_write': 1 }
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 16, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'max_read': config_file['num_cores'], 'max_write': config_file['num_cores'], 'prefetcher': 'no_llc', 'replacement': 'lru_llc' }
default_pmem = { 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'row_size': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5 }
default_vmem = { 'size': 8589934592, 'num_levels': 5 }

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

for i in range(len(config_file['ooo_cpu'])):
    config_file['ooo_cpu'][i] = merge_dicts(default_core, {'branch_predictor': config_file['branch_predictor']} if 'branch_predictor' in config_file else {}, config_file['ooo_cpu'][i])
    config_file['ooo_cpu'][i] = merge_dicts(default_core, {'btb': config_file['btb']} if 'btb' in config_file else {}, config_file['ooo_cpu'][i])
    config_file['ooo_cpu'][i]['DIB'] = merge_dicts(default_dib, config_file.get('DIB', {}), config_file['ooo_cpu'][i].get('DIB',{}))
    config_file['ooo_cpu'][i]['L1I'] = merge_dicts(default_l1i, config_file.get('L1I', {}), config_file['ooo_cpu'][i].get('L1I',{}))
    config_file['ooo_cpu'][i]['L1D'] = merge_dicts(default_l1d, config_file.get('L1D', {}), config_file['ooo_cpu'][i].get('L1D',{}))
    config_file['ooo_cpu'][i]['L2C'] = merge_dicts(default_l2c, config_file.get('L2C', {}), config_file['ooo_cpu'][i].get('L2C',{}))
    config_file['ooo_cpu'][i]['ITLB'] = merge_dicts(default_itlb, config_file.get('ITLB', {}), config_file['ooo_cpu'][i].get('ITLB',{}))
    config_file['ooo_cpu'][i]['DTLB'] = merge_dicts(default_dtlb, config_file.get('DTLB', {}), config_file['ooo_cpu'][i].get('DTLB',{}))
    config_file['ooo_cpu'][i]['STLB'] = merge_dicts(default_stlb, config_file.get('STLB', {}), config_file['ooo_cpu'][i].get('STLB',{}))

config_file['LLC'] = merge_dicts(default_llc, config_file.get('LLC',{}))
config_file['physical_memory'] = merge_dicts(default_pmem, config_file.get('physical_memory',{}))
config_file['virtual_memory'] = merge_dicts(default_vmem, config_file.get('virtual_memory',{}))

###
# Copy or trim cores as necessary to fill out the specified number of cores
###

config_file['ooo_cpu'] = list(itertools.islice(itertools.cycle(config_file['ooo_cpu']), config_file['num_cores']))

###
# Check to make sure modules exist and they correspond to any already-built modules.
###

# Associate modules with paths
libfilenames = {}
for i,cpu in enumerate(config_file['ooo_cpu'][:1]):
    if cpu['L1I']['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l1iprefetcher.a'] = 'prefetcher/' + cpu['L1I']['prefetcher']
    if cpu['L1D']['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l1dprefetcher.a'] = 'prefetcher/' + cpu['L1D']['prefetcher']
    if cpu['L2C']['prefetcher'] is not None:
        libfilenames['cpu' + str(i) + 'l2cprefetcher.a'] = 'prefetcher/' + cpu['L2C']['prefetcher']
    if cpu['branch_predictor'] is not None:
        libfilenames['cpu' + str(i) + 'branch_predictor.a'] = 'branch/' + cpu['branch_predictor']
    if cpu['btb'] is not None:
        libfilenames['cpu' + str(i) + 'btb.a'] = 'btb/' + cpu['btb']
if config_file['LLC']['prefetcher'] is not None:
    libfilenames['llprefetcher.a'] = 'prefetcher/' + config_file['LLC']['prefetcher']
if config_file['LLC']['replacement'] is not None:
    libfilenames['llreplacement.a'] = 'replacement/' + config_file['LLC']['replacement']

# Assert module paths exist
for path in libfilenames.values():
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
    wfp.write('#include "' + os.path.basename(constants_header_name) + '"\n')
    wfp.write('#include <vector>\n')

    wfp.write(llc_fmtstr.format(cpu='', name='LLC', attrs=config_file['LLC']))

    for i,cpu in enumerate(config_file['ooo_cpu']):
        wfp.write(cpu_fmtstr.format(cpu=i, attrs=cpu))

    wfp.write('std::vector<O3_CPU*> ooo_cpu {\n')
    for i in range(len(config_file['ooo_cpu'])):
        if i > 0:
            wfp.write(',\n')
        wfp.write('&cpu{}_inst'.format(i))
    wfp.write('\n};\n')

    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')

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
    wfp.write(define_fmtstr.format(name='cpu_clock_freq').format(names=const_names, config=config_file))

    # As a temporary measure, I am duplicating the existing setup that uses preprocessor defines.
    # Eventually, I would like to pass this information into the constructors.
    wfp.write('\n')
    wfp.write(cache_define_fmtstr.format(name='L1I', attrs=config_file['ooo_cpu'][0]['L1I']))
    wfp.write(cache_define_fmtstr.format(name='L1D', attrs=config_file['ooo_cpu'][0]['L1D']))
    wfp.write(cache_define_fmtstr.format(name='L2C', attrs=config_file['ooo_cpu'][0]['L2C']))
    wfp.write(cache_define_fmtstr.format(name='ITLB', attrs=config_file['ooo_cpu'][0]['ITLB']))
    wfp.write(cache_define_fmtstr.format(name='DTLB', attrs=config_file['ooo_cpu'][0]['DTLB']))
    wfp.write(cache_define_fmtstr.format(name='STLB', attrs=config_file['ooo_cpu'][0]['STLB']))
    wfp.write('\n')

    wfp.write('#define LLC_LATENCY {}u\n\n'.format(config_file['LLC']['latency']))

    for k in const_names['physical_memory']:
        if k in ['tRP', 'tRCD', 'tCAS']:
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
    wfp.write('CXXFLAGS := ' + config_file.get('CXXFLAGS', '-Wall -O3') + ' -std=c++11\n')
    wfp.write('CPPFLAGS := ' + config_file.get('CPPFLAGS', '') + ' -Iinc -MMD -MP\n')
    wfp.write('LDFLAGS := ' + config_file.get('LDFLAGS', '') + '\n')
    wfp.write('LDLIBS := ' + config_file.get('LDLIBS', '') + '\n')
    wfp.write('\n')
    wfp.write('.phony: all clean\n\n')
    wfp.write('all: ' + config_file['executable_name'] + '\n\n')
    wfp.write('clean: \n\t find . -name \*.o -delete\n\t find . -name \*.d -delete\n\t $(RM) -r obj\n\n')
    wfp.write(config_file['executable_name'] + ': $(patsubst %.cc,%.o,$(wildcard src/*.cc)) ' + ' '.join('obj/' + k for k in libfilenames) + '\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    for kv in libfilenames.items():
        wfp.write(module_make_fmtstr.format(*kv))

    wfp.write('obj/%.o: */%.c\n')
    wfp.write('\t@mkdir -p $(dir $@)\n')
    wfp.write('\t$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<\n\n')

    wfp.write('obj/%.o: */%.cc\n')
    wfp.write('\t@mkdir -p $(dir $@)\n')
    wfp.write('\t$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<\n\n')

    wfp.write('-include $(wildcard prefetcher/*/*.d)\n')
    wfp.write('-include $(wildcard branch/*/*.d)\n')
    wfp.write('-include $(wildcard btb/*/*.d)\n')
    wfp.write('-include $(wildcard replacement/*/*.d)\n')
    wfp.write('-include $(wildcard src/*.d)\n')

# Configuration cache
with open(config_cache_name, 'wt') as wfp:
    json.dump(libfilenames, wfp)

