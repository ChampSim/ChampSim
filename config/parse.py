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
import operator
import collections
import os
import math

from . import defaults
from . import modules
from . import util

default_root = { 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1 }
default_pmem = { 'name': 'DRAM', 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'lines_per_column': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'pte_page_size': (1 << 12), 'num_levels': 5, 'minor_fault_penalty': 200 }

cache_deprecation_keys = {
    'max_read': 'max_tag_check',
    'max_write': 'max_fill'
}

ptw_deprecation_keys = {
    'ptw_mshr_size': 'mshr_size',
    'ptw_max_read': 'max_read',
    'ptw_max_write': 'max_write',
    'ptw_rq_size': 'rq_size'
}

# Scale frequencies
def scale_frequencies(iterable):
    it_a, it_b = itertools.tee(iterable, 2)
    max_freq = max(element['frequency'] for element in it_a)
    for element in it_b:
        element['frequency'] = max_freq / element['frequency']

def executable_name(*config_list):
    name_parts = filter(None, ('champsim', *(c.get('name') for c in config_list)))
    name_specifications = reversed(list(filter(None, (c.get('executable_name') for c in config_list))))
    return next(name_specifications, '_'.join(name_parts))

def duplicate_to_length(elements, count):
    '''
    Duplicate an array of elements, truncating if the sequence is longer than the count

    > duplicate_to_length([1,2,3], 6)
    [1,1,2,2,3,3]

    > duplicate_to_length([1,2], 5)
    [1,1,1,2,2]

    > duplicate_to_length([1,2,3,4], 3)
    [1,2,3]

    :param elements: the sequence of elements to be duplicated
    :param count: the final length
    '''
    repeat_factor = math.ceil(count / len(elements))
    return list(itertools.islice(itertools.chain(*(itertools.repeat(e, repeat_factor) for e in elements)), count))

def filter_inaccessible(system, roots, key='lower_level'):
    '''
    Filters a system to the elements only accessible from the given roots by the given key.

    :param system: The system to filter
    :param roots: The roots from which to prune the system
    :param key: The key used to traverse the system
    '''
    return util.combine_named(*(util.iter_system(system, r, key=key) for r in roots))

def split_string_or_list(val, delim=','):
    ''' Split a comma-separated string into a list '''
    if isinstance(val, str):
        retval = (t.strip() for t in val.split(delim))
        return [v for v in retval if v]
    return val

def default_element_name(cpu, elem):
    return cpu['name'] + '_' + elem

def normalize_config(config_file):
    ''' Normalize a JSON configuration in preparation for parsing '''
    # Copy or trim cores as necessary to fill out the specified number of cores
    cores = duplicate_to_length(config_file.get('ooo_cpu', [{}]), config_file.get('num_cores', 1))

    # Default core elements
    core_keys_to_copy = ('frequency', 'ifetch_buffer_size', 'decode_buffer_size', 'dispatch_buffer_size', 'rob_size', 'lq_size', 'sq_size', 'fetch_width', 'decode_width', 'dispatch_width', 'execute_width', 'lq_width', 'sq_width', 'retire_width', 'mispredict_penalty', 'scheduler_size', 'decode_latency', 'dispatch_latency', 'schedule_latency', 'execute_latency', 'branch_predictor', 'btb', 'DIB')
    cores = [util.chain(cpu, util.subdict(config_file, core_keys_to_copy), {'name': 'cpu'+str(i)}) for i,cpu in enumerate(cores)]

    pinned_cache_names = ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB')
    caches = util.combine_named(
            config_file.get('caches', []),

            # Copy values from the core specification, if these are dicts
            (util.chain(core[name], { 'name': default_element_name(core,name) }) for core, name in itertools.product(cores, pinned_cache_names) if isinstance(core.get(name), dict)),

            # Copy values from the config root, if these are dicts
            (util.chain(config_file[name], { 'name': default_element_name(core,name) }) for core, name in itertools.product(cores, pinned_cache_names) if isinstance(config_file.get(name), dict))
            )

    # Read LLC from the configuration file
    if 'LLC' in config_file:
        caches.update(LLC={'name': 'LLC', **config_file['LLC']})

    ptws = util.combine_named(
            config_file.get('ptws',[]),

            # Copy values from the core specification, if these are dicts
            (util.chain(c['PTW'], { 'name': default_element_name(c, 'PTW') }) for c in cores if isinstance(c.get('PTW'), dict)),

            # Copy values from the config root, if these are dicts
            (util.chain(config_file['PTW'], { 'name': default_element_name(c, 'PTW') }) for c in cores if isinstance(config_file.get('PTW'), dict))
            )

    # Convert all core values to labels
    cores = [util.chain({n: cpu[n].get('name', default_element_name(cpu, n)) for n in (*pinned_cache_names, 'PTW') if isinstance(cpu.get(n), dict)}, cpu) for cpu in cores]

    # The name 'DRAM' is reserved for the physical memory
    caches = {k:v for k,v in caches.items() if k != 'DRAM'}

    return cores, caches, ptws, config_file.get('physical_memory', {}), config_file.get('virtual_memory', {})

def core_default_names(cpu):
    """ Apply defaults to a cpu with the given index """
    default_element_names = {n: default_element_name(cpu,n) for n in ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB', 'PTW')}
    default_core = {
        'frequency' : 4000,
        'DIB': {},
    }
    return util.chain(cpu, default_element_names, default_core)

def default_frequencies(cores, caches):
    '''
    Get frequencies as the maximum of the upper levels.

    :param cores: the list of cpu cores
    :param caches: the dictionary of caches
    '''
    def make_path(cpu, name):
        '''
        Make a path down 'caches' that has at least one frequency (from the cpu)

        :param cpu: a cpu core dictionary
        :param name: the cache key name
        '''
        base_path = util.iter_system(caches, cpu[name])
        freq_top = ({ 'frequency': cpu['frequency'] },)

        # put the cpu frequency as the default for the highest level (this does not override a provided value)
        path = itertools.starmap(util.chain, itertools.zip_longest(base_path, freq_top, fillvalue={}))

        # prune out everything but the name and frequency (if present)
        return (util.subdict(element, ('name', 'frequency')) for element in path)

    # Create a list of paths from the cores
    paths = itertools.starmap(make_path, itertools.product(cores, ('L1I', 'L1D', 'ITLB', 'DTLB')))

    # Propogate the frequencies down the path
    paths = (util.propogate_down(p, 'frequency') for p in paths)

    # Collect caches in multiple paths together
    aggregate = sorted(itertools.chain(*paths), key=operator.itemgetter('name'))
    aggregate = itertools.groupby(aggregate, key=operator.itemgetter('name'))

    # The frequency is the maximum of the frequencies seen
    # Note if the frequency was provided, it was never overwritten
    yield from [{ 'name': name, 'frequency': max(x['frequency'] for x in chunk) } for name, chunk in aggregate]

def do_deprecation(element, deprecation_map):
    '''
    Print a warning and return a replacement dictionary for keys that are deprecated.
    Currently only supports simple renamed keys

    :param element: the element to be examined
    :param deprecation_map: a dictionary mapping deprecated keys to new keys
    '''

    retval = { 'name': element['name'] }
    for old, new in deprecation_map.items():
        if old in element:
            print(f'WARNING: key "{old}" in element {element["name"]} is deprecated. Use "{new}" instead.')
            retval = { new: element[old], **retval }
    return retval

def parse_normalized(cores, caches, ptws, pmem, vmem, merged_configs, branch_context, btb_context, prefetcher_context, replacement_context, compile_all_modules):
    config_file = util.chain(merged_configs, default_root)

    pmem = util.chain(pmem, default_pmem)
    vmem = util.chain(vmem, default_vmem)

    # Give cores numeric indices and default cache names
    cores = [{'_index': i, **core_default_names(cpu)} for i,cpu in enumerate(cores)]

    l1i_path_names = tuple(cpu['L1I'] for cpu in cores)
    l1d_path_names = tuple(cpu['L1D'] for cpu in cores)
    itlb_path_names = tuple(cpu['ITLB'] for cpu in cores)
    dtlb_path_names = tuple(cpu['DTLB'] for cpu in cores)

    # Instantiate any missing default caches
    caches = util.combine_named(caches.values(), ({ 'name': 'LLC' },), *map(defaults.cache_core_defaults, cores))
    ptws = util.combine_named(ptws.values(), *map(defaults.ptw_core_defaults, cores))

    # Remove caches that are inaccessible
    caches = filter_inaccessible(caches, (*l1i_path_names, *l1d_path_names, *itlb_path_names, *dtlb_path_names))

    # Follow paths and apply default sizings
    caches = util.combine_named(
        caches.values(),
        defaults.list_defaults(cores, caches),
        default_frequencies(cores, caches)
    )

    tlb_path = itertools.chain(*(util.iter_system(caches, name) for name in (*itlb_path_names, *dtlb_path_names)))
    data_path = itertools.chain(*(util.iter_system(caches, name) for name in (*l1i_path_names, *l1d_path_names)))
    caches = util.combine_named(
        # Set prefetcher_activate
        ({ 'name': k,
           'prefetch_activate': split_string_or_list(cache['prefetch_activate'])
        } for k,cache in caches.items() if 'prefetch_activate' in cache),

        # TLBs use page offsets, Caches use block offsets
        ({'name': c['name'], '_offset_bits': f'champsim::lg2({config_file["page_size"]})'} for c in tlb_path),
        ({'name': c['name'], '_offset_bits': f'champsim::lg2({config_file["block_size"]})'} for c in data_path),

        caches.values(),

        ## DEPRECATION
        # The listed keys are deprecated. For now, permit them but print a warning
        (do_deprecation(cache, cache_deprecation_keys) for cache in caches.values()),

        # The end of the data path is the physical memory
        *((
            {'name': collections.deque(util.iter_system(caches, cpu['L1I']), maxlen=1)[0]['name'], 'lower_level': 'DRAM'},
            {'name': collections.deque(util.iter_system(caches, cpu['L1D']), maxlen=1)[0]['name'], 'lower_level': 'DRAM'},
            {'name': collections.deque(util.iter_system(caches, cpu['ITLB']), maxlen=1)[0]['name'], 'lower_level': cpu['PTW']},
            {'name': collections.deque(util.iter_system(caches, cpu['DTLB']), maxlen=1)[0]['name'], 'lower_level': cpu['PTW']}
         ) for cpu in cores),

        ({ 'name': k,
            # Mark queues that need to match full addresses on collision
           '_queue_check_full_addr': cache.get('_first_level', False) or cache.get('wq_check_full_addr', False),

            # Get module path names and unique module names
           '_replacement_data': [*map(replacement_context.find, util.wrap_list(cache.get('replacement', 'lru')))],
           '_prefetcher_data': [{
               '_is_instruction_prefetcher': cache.get('_is_instruction_cache',False),
               **prefetcher_context.find(f)
           } for f in util.wrap_list(cache.get('prefetcher', 'no_instr' if cache.get('_is_instruction_cache') else 'no'))]
        } for k,cache in caches.items())
    )

    ptws = util.combine_named(
        ptws.values(),

        ## DEPRECATION
        # The listed keys are deprecated. For now, permit them but print a warning
        (do_deprecation(ptw, ptw_deprecation_keys) for ptw in ptws.values()),

        ({ 'name': cpu['PTW'], 'frequency': cpu.get('frequency'), 'cpu': cpu.get('_index'),
           **defaults.ul_dependent_defaults(*util.upper_levels_for(caches.values(), cpu['PTW']), queue_factor=16, mshr_factor=5, bandwidth_factor=2)
         } for cpu in cores)
    )

    cores = list(util.combine_named(cores,
        ({
            'name': c['name'],
            '_branch_predictor_data': [*map(branch_context.find, util.wrap_list(c.get('branch_predictor', 'hashed_perceptron')))],
            '_btb_data': [*map(btb_context.find, util.wrap_list(c.get('btb', 'basic_btb')))]
         } for c in cores),
        ).values()
    )

    pmem['io_freq'] = pmem['frequency'] # Save value
    scale_frequencies(itertools.chain(cores, caches.values(), ptws.values(), (pmem,)))

    elements = {'cores': cores, 'caches': tuple(caches.values()), 'ptws': tuple(ptws.values()), 'pmem': pmem, 'vmem': vmem}
    module_info = {
        'repl': util.combine_named(*(c['_replacement_data'] for c in caches.values()), replacement_context.find_all()),
        'pref': util.combine_named(*(c['_prefetcher_data'] for c in caches.values()), prefetcher_context.find_all()),
        'branch': util.combine_named(*(c['_branch_predictor_data'] for c in cores), branch_context.find_all()),
        'btb': util.combine_named(*(c['_btb_data'] for c in cores), btb_context.find_all())
    }

    if compile_all_modules:
        modules_to_compile = [*set(itertools.chain(*(d.keys() for d in module_info.values())))]
    else:
        modules_to_compile = [*set(d['name'] for d in itertools.chain(
            *(c['_replacement_data'] for c in caches.values()),
            *(c['_prefetcher_data'] for c in caches.values()),
            *(c['_branch_predictor_data'] for c in cores),
            *(c['_btb_data'] for c in cores)
        ))]

    env_vars = ('CC', 'CXX', 'CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')
    extern_config_file_keys = ('block_size', 'page_size', 'heartbeat_frequency', 'num_cores')

    return elements, modules_to_compile, module_info, util.subdict(config_file, extern_config_file_keys), util.subdict(config_file, env_vars)

def parse_config(*configs, module_dir=None, branch_dir=None, btb_dir=None, pref_dir=None, repl_dir=None, compile_all_modules=False):
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    def list_dirs(dirname, var):
        return [*(os.path.join(m,dirname) for m in (module_dir or [])), *var, os.path.join(champsim_root,dirname)]

    local_branch_dir = list_dirs('branch', branch_dir or [])
    local_btb_dir = list_dirs('btb', btb_dir or [])
    local_repl_dir = list_dirs('replacement', repl_dir or [])
    local_pref_dir = list_dirs('prefetcher', pref_dir or [])

    name = executable_name(*configs)
    merged_configs = util.chain(*configs)
    elements, modules_to_compile, module_info, config_file, env = parse_normalized(*normalize_config(merged_configs),
        merged_configs,
        branch_context = modules.ModuleSearchContext(local_branch_dir),
        btb_context = modules.ModuleSearchContext(local_btb_dir),
        replacement_context = modules.ModuleSearchContext(local_repl_dir),
        prefetcher_context = modules.ModuleSearchContext(local_pref_dir),
        compile_all_modules = compile_all_modules
    )

    module_info = {
            'repl': {k: util.chain(v, modules.get_repl_data(v['name'])) for k,v in module_info['repl'].items()},
            'pref': {k: util.chain(v, modules.get_pref_data(v['name'], v['_is_instruction_prefetcher'])) for k,v in module_info['pref'].items()},
            'branch': {k: util.chain(v, modules.get_branch_data(v['name'])) for k,v in module_info['branch'].items()},
            'btb': {k: util.chain(v, modules.get_btb_data(v['name'])) for k,v in module_info['btb'].items()},
            }

    return name, elements, modules_to_compile, module_info, config_file, env
