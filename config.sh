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

# (cpu, name, **attrs)
llc_fmtstr = 'CACHE {name}("{name}", {attrs[sets]}, {attrs[ways]}, {attrs[wq_size]}, {attrs[rq_size]}, {attrs[pq_size]}, {attrs[mshr_size]}, {attrs[latency]});\n'

cpu_fmtstr = 'O3_CPU cpu{cpu}({cpu}, {attrs[rob_size]}, {attrs[lq_size]}, {attrs[sq_size]}, {attrs[fetch_width]}, {attrs[decode_width]}, {attrs[execute_width]}, {attrs[retire_width]}, {attrs[mispredict_penalty]}, {attrs[decode_latency]}, {attrs[schedule_latency]}, {attrs[execute_latency]}, {attrs[DIB][sets]}, {attrs[DIB][ways]}, &cpu{cpu}L1I, &cpu{cpu}L1D, &cpu{cpu}L2C, &cpu{cpu}ITLB, &cpu{cpu}DTLB, &cpu{cpu}STLB);\n'

pmem_fmtstr = 'MEMORY_CONTROLLER DRAM("DRAM");\n'
vmem_fmtstr = 'VirtualMemory vmem(NUM_CPUS, {attrs[size]}, PAGE_SIZE, {attrs[num_levels]}, 1);\n'

prefetcher_make_fmtstr = 'obj/{}: $(wildcard prefetcher/{}/*.cc)\n\t@mkdir -p $(dir $@)\n\t$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $^ -o $@\n\n'
replacement_make_fmtstr = 'obj/{}: $(wildcard replacement/{}/*.cc)\n\t@mkdir -p $(dir $@)\n\t$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $^ -o $@\n\n'
branch_predictor_make_fmtstr = 'obj/{}: $(wildcard branch/{}/*.cc)\n\t@mkdir -p $(dir $@)\n\t$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $^ -o $@\n\n'

define_fmtstr = '#define {{names[{name}]}} {{config[{name}]}}\n'
define_log_fmtstr = '#define LOG2_{{names[{name}]}} lg2({{config[{name}]}})\n'
cache_define_fmtstr = '#define {name}_SET {attrs[sets]}\n#define {name}_WAY {attrs[ways]}\n#define {name}_WQ_SIZE {attrs[wq_size]}\n#define {name}_RQ_SIZE {attrs[rq_size]}\n#define {name}_PQ_SIZE {attrs[pq_size]}\n#define {name}_MSHR_SIZE {attrs[mshr_size]}\n#define {name}_LATENCY {attrs[latency]}\n'

const_names = {
    'block_size': 'BLOCK_SIZE',
    'page_size': 'PAGE_SIZE',
    'heartbeat_frequency': 'STAT_PRINTING_PERIOD',
    'cpu_clock_freq': 'CPU_FREQ',
    'num_cores': 'NUM_CPUS',
    'core': {
        'rob_size': 'ROB_SIZE',
        'lq_size': 'LQ_SIZE',
        'sq_size': 'SQ_SIZE',
        'fetch_width' : 'FETCH_WIDTH',
        'decode_width' : 'DECODE_WIDTH',
        'execute_width' : 'EXEC_WIDTH',
        'lq_width' : 'LQ_WIDTH',
        'sq_width' : 'SQ_WIDTH',
        'retire_width' : 'RETIRE_WIDTH',
        'mispredict_penalty' : 'BRANCH_MISPREDICT_PENALTY',
        'scheduler_size' : 'SCHEDULER_SIZE',
        'decode_latency' : 'DECODE_LATENCY',
        'schedule_latency' : 'SCHEDULING_LATENCY',
        'execute_latency' : 'EXEC_LATENCY',
        'DIB' : {
            'sets' : 'DIB_SET',
            'ways' : 'DIB_WAY'
        }
    },
    'physical_memory': {
        'frequency': 'DRAM_IO_FREQ',
        'channels': 'DRAM_CHANNELS',
        'ranks': 'DRAM_RANKS',
        'banks': 'DRAM_BANKS',
        'rows': 'DRAM_ROWS',
        'columns': 'DRAM_COLUMNS',
        'row_size': 'DRAM_ROW_SIZE'
    }
}

default_root = { 'executable_name': 'bin/champsim', 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'cpu_clock_freq' : 4000, 'num_cores': 1, 'ooo_cpu': [{}] }
config_file = merge_dicts(default_root, config_file) # doing this early because LLC dimensions depend on it

default_core = { 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 2, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal' }
default_dib  = { 'sets': 8, 'ways': 8 }
default_l1i  = { 'sets': 64, 'ways': 8, 'rq_size': 64, 'wq_size': 64, 'pq_size': 32, 'mshr_size': 8, 'latency': 4, 'prefetcher': 'no_l1i' }
default_l1d  = { 'sets': 64, 'ways': 12, 'rq_size': 64, 'wq_size': 64, 'pq_size': 8, 'mshr_size': 16, 'latency': 5, 'prefetcher': 'no_l1d' }
default_l2c  = { 'sets': 1024, 'ways': 8, 'rq_size': 32, 'wq_size': 32, 'pq_size': 16, 'mshr_size': 32, 'latency': 10, 'prefetcher': 'no_l2c' }
default_itlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1 }
default_dtlb = { 'sets': 16, 'ways': 4, 'rq_size': 16, 'wq_size': 16, 'pq_size': 0, 'mshr_size': 8, 'latency': 1 }
default_stlb = { 'sets': 128, 'ways': 12, 'rq_size': 32, 'wq_size': 32, 'pq_size': 0, 'mshr_size': 16, 'latency': 8 }
default_llc  = { 'sets': 2048*config_file['num_cores'], 'ways': 8, 'rq_size': 32*config_file['num_cores'], 'wq_size': 32*config_file['num_cores'], 'pq_size': 32*config_file['num_cores'], 'mshr_size': 64*config_file['num_cores'], 'latency': 20, 'prefetcher': 'no_llc', 'replacement': 'lru_llc' }
default_pmem = { 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'row_size': 8 }
default_vmem = { 'size': 8589934592, 'num_levels': 5 }

# Make sure directories are present
os.makedirs(os.path.dirname(config_file['executable_name']), exist_ok=True)
os.makedirs(os.path.dirname(instantiation_file_name), exist_ok=True)
os.makedirs(os.path.dirname(constants_header_name), exist_ok=True)

# Establish default optional values
for i in range(len(config_file['ooo_cpu'])):
    config_file['ooo_cpu'][i] = merge_dicts(default_core, config_file['ooo_cpu'][i])
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

# Copy or trim cores as necessary to fill out the specified number of cores
config_file['ooo_cpu'] = list(itertools.islice(itertools.repeat(*config_file['ooo_cpu']), config_file['num_cores']))

# Begin file writing
with open(instantiation_file_name, 'wt') as wfp:
    wfp.write('/***\n * THIS FILE IS AUTOMATICALLY GENERATED\n * Do not edit this file. It will be overwritten when the configure script is run.\n ***/\n\n')
    wfp.write('#include "cache.h"\n')
    wfp.write('#include "champsim.h"\n')
    wfp.write('#include "dram_controller.h"\n')
    wfp.write('#include "ooo_cpu.h"\n')
    wfp.write('#include "vmem.h"\n')
    wfp.write('#include "' + os.path.basename(constants_header_name) + '"\n')
    wfp.write('#include <array>\n')

    wfp.write(llc_fmtstr.format(cpu='', name='LLC', attrs=config_file['LLC']))

    wfp.write('std::array<O3_CPU, NUM_CPUS> ooo_cpu { ')
    wfp.write(' };\n')

    wfp.write(pmem_fmtstr.format(attrs=config_file['physical_memory']))
    wfp.write(vmem_fmtstr.format(attrs=config_file['virtual_memory']))
    wfp.write('\n')

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
    for k,v in config_file['ooo_cpu'][0].items():
        if isinstance(v,dict):
            if k is 'DIB':
                wfp.write(define_fmtstr.format(name='sets').format(names=const_names['core']['DIB'], config=config_file['ooo_cpu'][0]['DIB']))
                wfp.write(define_fmtstr.format(name='ways').format(names=const_names['core']['DIB'], config=config_file['ooo_cpu'][0]['DIB']))
            else:
                wfp.write(cache_define_fmtstr.format(name=k, attrs=v))
            wfp.write('\n')
        else:
            if k is not 'branch_predictor':
                wfp.write(define_fmtstr.format(name=k).format(names=const_names['core'], config=config_file['ooo_cpu'][0]))

    wfp.write(cache_define_fmtstr.format(name='LLC', attrs=config_file['LLC']) + '\n')

    for k in const_names['physical_memory']:
        wfp.write(define_fmtstr.format(name=k).format(names=const_names['physical_memory'], config=config_file['physical_memory']))
        if k is not 'frequency':
            wfp.write(define_log_fmtstr.format(name=k).format(names=const_names['physical_memory'], config=config_file['physical_memory']))

    wfp.write('#endif\n')

with open('Makefile', 'wt') as wfp:
    wfp.write('CC := ' + config_file.get('CC', 'gcc') + '\n')
    wfp.write('CXX := ' + config_file.get('CXX', 'g++') + '\n')
    wfp.write('CFLAGS := ' + config_file.get('CFLAGS', '-Wall -O3') + ' -std=gnu99\n')
    wfp.write('CXXFLAGS := ' + config_file.get('CXXFLAGS', '-Wall -O3') + ' -std=c++11\n')
    wfp.write('CPPFLAGS := ' + config_file.get('CPPFLAGS', '') + ' -Iinc -MMD -MP\n')
    wfp.write('LDFLAGS := ' + config_file.get('LDFLAGS', '') + '\n')
    wfp.write('LDLIBS := ' + config_file.get('LDLIBS', '') + '\n')
    wfp.write('\n')
    wfp.write('obj_of = $(addsuffix .o, $(basename $(addprefix obj/,$(notdir $(1)))))\n')
    wfp.write('.phony: all clean\n\n')
    wfp.write('all: ' + config_file['executable_name'] + '\n\n')
    wfp.write('clean: \n\t $(RM) -r obj\n\n')
    wfp.write(config_file['executable_name'] + ': $(call obj_of,$(wildcard src/*.cc)) obj/l1iprefetcher.o obj/l1dprefetcher.o obj/l2cprefetcher.o obj/llprefetcher.o obj/llreplacement.o obj/branch_predictor.o $(call obj_of, ' + instantiation_file_name + ')\n')
    wfp.write('\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n')

    #TODO heterogeneous cores
    wfp.write(prefetcher_make_fmtstr.format('l1iprefetcher.o', config_file['ooo_cpu'][0]['L1I']['prefetcher']))
    wfp.write(prefetcher_make_fmtstr.format('l1dprefetcher.o', config_file['ooo_cpu'][0]['L1D']['prefetcher']))
    wfp.write(prefetcher_make_fmtstr.format('l2cprefetcher.o', config_file['ooo_cpu'][0]['L2C']['prefetcher']))
    wfp.write(prefetcher_make_fmtstr.format('llprefetcher.o',  config_file['LLC']['prefetcher']))
    wfp.write(replacement_make_fmtstr.format('llreplacement.o', config_file['LLC']['replacement']))
    wfp.write(branch_predictor_make_fmtstr.format('branch_predictor.o', config_file['ooo_cpu'][0]['branch_predictor']))

    wfp.write('obj/%.o: */%.c\n')
    wfp.write('\t@mkdir -p $(dir $@)\n')
    wfp.write('\t$(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<\n\n')

    wfp.write('obj/%.o: */%.cc\n')
    wfp.write('\t@mkdir -p $(dir $@)\n')
    wfp.write('\t$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<\n\n')

    wfp.write('-include $(wildcard obj/*.d)\n')

