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

import itertools
import os
import math

from . import defaults
from . import modules
from . import util

default_root = { 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1 }
default_core = { 'frequency' : 4000, 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_pmem = { 'name': 'DRAM', 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'lines_per_column': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'pte_page_size': (1 << 12), 'num_levels': 5, 'minor_fault_penalty': 200 }

# Assign defaults that are unique per core
def upper_levels_for(system, names):
    upper_levels = sorted(system, key=lambda x: x.get('lower_level', ''))
    upper_levels = itertools.groupby(upper_levels, key=lambda x: x.get('lower_level', ''))
    yield from ((k,v) for k,v in upper_levels if k in names)

# Scale frequencies
def scale_frequencies(it):
    it_a, it_b = itertools.tee(it, 2)
    max_freq = max(x['frequency'] for x in it_a)
    for x in it_b:
        x['frequency'] = max_freq / x['frequency']

def parse_config(*configs, module_dir=[], branch_dir=[], btb_dir=[], pref_dir=[], repl_dir=[], compile_all_modules=False):
    name_parts = ['champsim', *(c.get('name') for c in configs if c.get('name') is not None)]

    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    branch_search_dirs = [*(os.path.join(m, 'branch') for m in module_dir), *branch_dir, os.path.join(champsim_root, 'branch')]
    btb_search_dirs = [*(os.path.join(m, 'btb') for m in module_dir), *btb_dir, os.path.join(champsim_root, 'btb')]
    prefetcher_search_dirs = [*(os.path.join(m, 'prefetcher') for m in module_dir), *pref_dir, os.path.join(champsim_root, 'prefetcher')]
    replacement_search_dirs = [*(os.path.join(m, 'replacement') for m in module_dir), *repl_dir, os.path.join(champsim_root, 'replacement')]

    config_file = util.chain(*configs, default_root)

    pmem = util.chain(config_file.get('physical_memory', {}), default_pmem)
    vmem = util.chain(config_file.get('virtual_memory', {}), default_vmem)

    cores = config_file.get('ooo_cpu', [{}])

    # Copy or trim cores as necessary to fill out the specified number of cores
    cpu_repeat_factor = math.ceil(config_file['num_cores'] / len(cores));
    cores = list(itertools.islice(itertools.chain.from_iterable(itertools.repeat(c, cpu_repeat_factor) for c in cores), config_file['num_cores']))

    # Default core elements
    cores = [util.chain(cpu, {'name': 'cpu'+str(i), 'index': i, 'DIB': config_file.get('DIB',{})}, {'DIB': default_dib}, default_core) for i,cpu in enumerate(cores)]

    # Establish defaults for first-level caches
    caches = util.combine_named(
            config_file.get('caches', []),
            # Copy values from the core specification and config root, if these are dicts
            ({'name': util.read_element_name(*cn), **cn[0][cn[1]]} for cn in itertools.product(cores, ('L1I', 'L1D', 'ITLB', 'DTLB')) if isinstance(cn[0].get(cn[1]), dict)),
            ({'name': util.read_element_name(*cn), **config_file[cn[1]]} for cn in itertools.product(cores, ('L1I', 'L1D', 'ITLB', 'DTLB')) if isinstance(config_file.get(cn[1]), dict)),
            # Apply defaults named after the cores
            map(defaults.named_l1i_defaults, cores),
            map(defaults.named_l1d_defaults, cores),
            map(defaults.named_itlb_defaults, cores),
            map(defaults.named_dtlb_defaults, cores)
            )

    cores = [util.chain({n: util.read_element_name(cpu, n) for n in ('L1I', 'L1D', 'ITLB', 'DTLB')}, cpu) for cpu in cores]

    # Establish defaults for second-level caches
    caches = util.combine_named(
            caches.values(),
            # Copy values from the core specification and config root, if these are dicts
            ({'name': util.read_element_name(*cn), **cn[0][cn[1]]} for cn in itertools.product(cores, ('L2C', 'STLB')) if isinstance(cn[0].get(cn[1]), dict)),
            ({'name': util.read_element_name(*cn), **config_file[cn[1]]} for cn in itertools.product(cores, ('L2C', 'STLB')) if isinstance(config_file.get(cn[1]), dict)),
            # Apply defaults named after the second-level caches
            (defaults.sequence_l2c_defaults(*ul) for ul in upper_levels_for(caches.values(), [caches[c['L1D']]['lower_level'] for c in cores])),
            (defaults.sequence_stlb_defaults(*ul) for ul in upper_levels_for(caches.values(), [caches[c['DTLB']]['lower_level'] for c in cores])),
            # Apply defaults named after the cores
            map(defaults.named_l2c_defaults, cores),
            map(defaults.named_stlb_defaults, cores),
            )

    # Establish defaults for third-level caches
    ptws = util.combine_named(
                     config_file.get('ptws',[]),
                     ({'name': util.read_element_name(c,'PTW'), **c['PTW']} for c in cores if isinstance(c.get('PTW'), dict)),
                     ({'name': util.read_element_name(c,'PTW'), **config_file['PTW']} for c in cores if isinstance(config_file.get('PTW'), dict)),
                     map(defaults.named_ptw_defaults, cores),
                    )

    caches = util.combine_named(
            caches.values(),
            ({'name': 'LLC', **config_file.get('LLC', {})},),
            (defaults.named_llc_defaults(*ul) for ul in upper_levels_for(caches.values(), [caches[caches[c['L1D']]['lower_level']]['lower_level'] for c in cores]))
            )

    # The name 'DRAM' is reserved for the physical memory
    caches = {k:v for k,v in caches.items() if k != 'DRAM'}

    ## DEPRECATION
    # The keys "max_read" and "max_write" are deprecated. For now, permit them but print a warning
    for cache in caches.values():
        if "max_read" in cache:
            print('WARNING: key "max_read" in cache ', cache['name'], ' is deprecated. Use "max_tag_check" instead.')
            cache['max_tag_check'] = cache['max_read']
        if "max_write" in cache:
            print('WARNING: key "max_write" in cache ', cache['name'], ' is deprecated. Use "max_fill" instead.')
            cache['max_fill'] = cache['max_write']

    # Remove caches that are inaccessible
    caches = util.combine_named(*(util.iter_system(caches, cpu[name]) for cpu,name in itertools.product(cores, ('ITLB', 'DTLB', 'L1I', 'L1D'))))

    # Establish latencies in caches
    caches = util.combine_named(caches.values(), ({'name': c['name'], 'hit_latency': (c.get('latency',100) - c['fill_latency'])} for c in caches.values()))

    pmem['io_freq'] = pmem['frequency'] # Save value
    scale_frequencies(itertools.chain(cores, caches.values(), ptws.values(), (pmem,)))

    # TLBs use page offsets, Caches use block offsets
    tlb_path = itertools.chain.from_iterable(util.iter_system(caches, cpu[name]) for cpu,name in itertools.product(cores, ('ITLB', 'DTLB')))
    l1d_path = itertools.chain.from_iterable(util.iter_system(caches, cpu[name]) for cpu,name in itertools.product(cores, ('L1I', 'L1D')))
    caches = util.combine_named(
            ({'name': c['name'], '_offset_bits': 'champsim::lg2(' + str(config_file['page_size']) + ')', '_needs_translate': False} for c in tlb_path),
            ({'name': c['name'], '_offset_bits': 'champsim::lg2(' + str(config_file['block_size']) + ')', '_needs_translate': c.get('_needs_translate', False) or c.get('virtual_prefetch', False)} for c in l1d_path),
            caches.values()
            )

    # Get module path names and unique module names
    caches = util.combine_named(caches.values(), ({
            'name': c['name'],
            '_replacement_modpaths': [modules.default_dir(replacement_search_dirs, f) for f in util.wrap_list(c.get('replacement', []))],
            '_prefetcher_modpaths':  [modules.default_dir(prefetcher_search_dirs, f) for f in util.wrap_list(c.get('prefetcher', []))],
            '_replacement_modnames': [modules.get_module_name(modules.default_dir(replacement_search_dirs, f)) for f in util.wrap_list(c.get('replacement', []))],
            '_prefetcher_modnames':  [modules.get_module_name(modules.default_dir(prefetcher_search_dirs, f)) for f in util.wrap_list(c.get('prefetcher', []))]
            } for c in caches.values()))

    cores = list(util.combine_named(cores, ({
            'name': c['name'],
            '_branch_predictor_modpaths': [modules.default_dir(branch_search_dirs, f) for f in util.wrap_list(c.get('branch_predictor', []))],
            '_btb_modpaths':  [modules.default_dir(btb_search_dirs, f) for f in util.wrap_list(c.get('btb', []))],
            '_branch_predictor_modnames': [modules.get_module_name(modules.default_dir(branch_search_dirs, f)) for f in util.wrap_list(c.get('branch_predictor', []))],
            '_btb_modnames':  [modules.get_module_name(modules.default_dir(btb_search_dirs, f)) for f in util.wrap_list(c.get('btb', []))]
            } for c in cores)).values())

    repl_data   = modules.get_module_data('_replacement_modnames', '_replacement_modpaths', caches.values(), replacement_search_dirs, modules.get_repl_data);
    pref_data   = modules.get_module_data('_prefetcher_modnames', '_prefetcher_modpaths', caches.values(), prefetcher_search_dirs, modules.get_pref_data);
    branch_data = modules.get_module_data('_branch_predictor_modnames', '_branch_predictor_modpaths', cores, branch_search_dirs, modules.get_branch_data);
    btb_data    = modules.get_module_data('_btb_modnames', '_btb_modpaths', cores, btb_search_dirs, modules.get_btb_data);

    if not compile_all_modules:
        repl_data = util.subdict(repl_data, list(itertools.chain(*(c['_replacement_modnames'] for c in caches.values()))))
        pref_data = util.subdict(pref_data, list(itertools.chain(*(c['_prefetcher_modnames'] for c in caches.values()))))
        branch_data = util.subdict(branch_data, list(itertools.chain(*(c['_branch_predictor_modnames'] for c in cores))))
        btb_data = util.subdict(btb_data, list(itertools.chain(*(c['_btb_modnames'] for c in cores))))

    elements = {'cores': cores, 'caches': tuple(caches.values()), 'ptws': tuple(ptws.values()), 'pmem': pmem, 'vmem': vmem}
    module_info = {'repl': dict(repl_data.items()), 'pref': dict(pref_data.items()), 'branch': dict(branch_data.items()), 'btb': dict(btb_data.items())}

    executable = config_file.get('executable_name', '_'.join(name_parts))

    env_vars = ('CC', 'CXX', 'CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')
    extern_config_file_keys = ('block_size', 'page_size', 'heartbeat_frequency', 'num_cores')

    return executable, elements, module_info, util.subdict(config_file, extern_config_file_keys), util.subdict(config_file, env_vars)

