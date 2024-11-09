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
import functools
import operator
import os
import math
from collections import deque

from . import defaults
from . import modules
from . import util

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

pmem_deprecation_keys = {
    'columns': 'bank_columns',
    'rows': 'bank_rows'
}

pmem_deprecation_warnings = {
    'columns': 'Set "bank_columns" to "columns" * 8'
}

def executable_name(*config_list):
    ''' Produce the executable name from a list of configurations '''
    name_parts = filter(None, ('champsim', *(c.get('name') for c in config_list)))
    name_specifications = reversed(list(filter(None, (c.get('executable_name') for c in config_list))))
    return next(name_specifications, '_'.join(name_parts))

def duplicate_to_length(elements, count):
    '''
    Duplicate an array of elements, truncating if the sequence is longer than the count

    >>> duplicate_to_length([1,2,3], 6)
    [1,1,2,2,3,3]
    >>> duplicate_to_length([1,2], 5)
    [1,1,1,2,2]
    >>> duplicate_to_length([1,2,3,4], 3)
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

def module_parse(mod, context):
    '''
    Parse a module descriptor in the configuration.

    If the descriptor is a dictionary, it should have at least a "path" key, and optionally a "class" and "legacy" key.
    If the descriptor is a string, it will be interpreted as a path (absolute or relative to the search paths)

    :param mod: the value from the configuration file
    :param context: an instance of modules.ModuleSearchContext, the search context for the module
    '''

    if isinstance(mod, dict):
        return util.chain(util.subdict(mod, ('class','legacy')), context.find(mod['path']))
    return context.find(mod)

def split_string_or_list(val, delim=','):
    ''' Split a comma-separated string into a list '''
    if isinstance(val, str):
        retval = (t.strip() for t in val.split(delim))
        return [v for v in retval if v]
    return val

def int_or_prefixed_size(val):
    '''
    Convert a string value to an integer. The following sizes are recognized:
        B, k, kB, kiB, M, MB, MiB, G, GB, GiB, T, TB, TiB
    '''

    sizes = {
        'k': 1024**1, 'kB': 1024**1, 'kiB': 1024**1,
        'M': 1024**2, 'MB': 1024**2, 'MiB': 1024**2,
        'G': 1024**3, 'GB': 1024**3, 'GiB': 1024**3,
        'T': 1024**4, 'TB': 1024**4, 'TiB': 1024**4,
        'B': 1
    }
    if isinstance(val, str):
        for suffix, multiplier in sizes.items():
            if val.endswith(suffix):
                return int(val[:-len(suffix)]) * multiplier
        return int(val)
    return val

def core_default_names(cpu):
    """ Apply defaults to a cpu with the given index """
    default_element_names = {n: f'{cpu["name"]}_{n}' for n in ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB', 'PTW')}
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
    paths = itertools.chain.from_iterable(util.propogate_down(p, 'frequency') for p in paths)

    # Collect caches in multiple paths together
    # Note if the frequency was provided, it was never overwritten
    def max_joiner(lhs, rhs):
        ''' The frequency is the maximum of the frequencies seen. '''
        return { 'name': lhs['name'], 'frequency': max(lhs['frequency'], rhs['frequency']) }

    yield from util.collect(paths, operator.itemgetter('name'), functools.partial(functools.reduce, max_joiner))

def do_deprecation(element, deprecation_map, warning_msg_map={}):
    '''
    Print a warning and return a replacement dictionary for keys that are deprecated.
    Currently only supports simple renamed keys

    :param element: the element to be examined
    :param deprecation_map: a dictionary mapping deprecated keys to new keys
    '''

    retval = { 'name': element['name'] }
    for old, new in deprecation_map.items():
        if old in element:
            print(f'WARNING: key "{old}" in element {element["name"]} is deprecated. Use "{new}" instead. {warning_msg_map.get(old, "")}')
            retval = { new: element[old], **retval }
    return retval

def path_end_in(path, end_name, key='lower_level'):
    return {'name': deque(path, maxlen=1)[0]['name'], key: end_name}

def extract_element(key, *parents):
    '''
    Extract a certain key from the series of parents, returning the merged keys.
    Keys whose values are not dictionaries are ignored.

    >>> a = { 'key': { 'internal': 1 } }
    >>> b = { 'key': { 'internal': 2 } }
    >>> extract_element('key', a, b)
    { 'internal': 1 }
    >>> c = { 'key': { 'other': 1 } }
    >>> extract_element('key', a, c)
    { 'internal': 1, 'other': 1 }
    >>> d = { 'key': 'foo' }
    >>> extract_element('key', a, c, d)
    { 'internal': 1, 'other': 1 }

    If one or more of the parents contains a 'name' key, the result will contain a 'name' key
    with value '{parent["name"]}_{key}'.

    :param key: the key to extract
    :param parents: the dictionaries to extract from
    '''

    parent_names = map(operator.methodcaller('get', 'name'), parents)
    child_names = map(lambda name: { 'name': f'{name}_{key}' }, filter(None, parent_names))

    local_elements = filter(lambda x: isinstance(x, dict) and x, map(operator.methodcaller('get', key), parents))
    local_elements = map(util.chain, local_elements, itertools.chain(child_names, itertools.repeat({})))

    return util.chain(*local_elements)

class NormalizedConfiguration:
    '''
    The internal representation of a JSON configuration.

    First, the configuration is normalized. It can then be merged with any number of configurations,
    and defaults are inferred when writing generated files.
    '''

    def __init__(self, config_file, verbose=False):
        ''' Normalize a JSON configuration in preparation for parsing '''
        # Copy or trim cores as necessary to fill out the specified number of cores
        self.cores = duplicate_to_length(config_file.get('ooo_cpu', [{}]), config_file.get('num_cores', 1))

        # Default core elements
        core_from_config = util.subdict(config_file,
            (
                'frequency', 'ifetch_buffer_size', 'decode_buffer_size', 'dispatch_buffer_size', 'register_file_size', 'rob_size', 'lq_size',
                'sq_size', 'fetch_width', 'decode_width', 'dispatch_width', 'execute_width', 'lq_width', 'sq_width',
                'retire_width', 'mispredict_penalty', 'scheduler_size', 'decode_latency', 'dispatch_latency',
                'schedule_latency', 'execute_latency', 'branch_predictor', 'btb', 'DIB'
            )
        )
        self.cores = [util.chain(cpu, core_from_config, {'name': f'cpu{i}'}) for i,cpu in enumerate(self.cores)]

        if verbose:
            print('P: core count', len(self.cores))

        pinned_cache_names = ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB')
        self.caches = util.combine_named(
            config_file.get('caches', []),
            (extract_element(name, core, config_file) for core, name in itertools.product(self.cores, pinned_cache_names))
        )

        # Read LLC from the configuration file
        if 'LLC' in config_file:
            self.caches.update(LLC={'name': 'LLC', **config_file['LLC']})

        if verbose:
            print('P: caches', list(self.caches.keys()))

        self.ptws = util.combine_named(
            config_file.get('ptws',[]),
            (extract_element('PTW', core, config_file) for core in self.cores)
        )

        if verbose:
            print('P: ptws', list(self.ptws.keys()))

        # Convert all core values to labels
        self.cores = [
            util.chain(*(
                {n: cpu[n].get('name', f'{cpu["name"]}_{n}')}
                for n in (*pinned_cache_names, 'PTW') if isinstance(cpu.get(n), dict)
            ),
            cpu
        ) for cpu in self.cores]

        # The name 'DRAM' is reserved for the physical memory
        self.caches = {k:v for k,v in self.caches.items() if k != 'DRAM'}

        self.pmem = config_file.get('physical_memory', {})
        
        #this allows frequency to be specified instead of data rate or vice-versa for DRAM
        if('frequency' in self.pmem.keys()):
            self.pmem['data_rate'] = self.pmem['frequency']
            self.pmem['frequency'] = self.pmem['frequency']/2
        elif('data_rate' in self.pmem.keys()):
            self.pmem['frequency'] = self.pmem['data_rate']/2

        if verbose:
            print('P: pmem', list(self.pmem.keys()))

        self.vmem = config_file.get('virtual_memory', {})

        if verbose:
            print('P: vmem', list(self.vmem.keys()))

        self.root = util.subdict(config_file,
            ('block_size', 'page_size', 'heartbeat_frequency')
        )

    def merge(self, rhs):
        ''' Merge another configuration into this one '''
        self.cores = list(itertools.starmap(util.chain, itertools.zip_longest(self.cores, rhs.cores, fillvalue={})))
        self.caches = util.chain(self.caches, rhs.caches)
        self.ptws = util.chain(self.ptws, rhs.ptws)
        self.pmem = util.chain(self.pmem, rhs.pmem)
        self.vmem = util.chain(self.vmem, rhs.vmem)
        self.root = util.chain(self.root, rhs.root)

    def apply_defaults_in(self, branch_context, btb_context, prefetcher_context, replacement_context, verbose=False):
        ''' Apply defaults and produce a result suitible for writing the generated files. '''
        if verbose:
            print('D: keys in root', list(self.root.keys()))
            for cpu in self.cores:
                print('D: core', cpu['name'], list(cpu.keys()))
            for cache in self.caches.values():
                print('D: cache', cache['name'], list(cache.keys()))
            for ptw in self.ptws.values():
                print('D: ptw', ptw['name'], list(ptw.keys()))

        def transform_for_keys(element, keys, transform_func):
            return { k:transform_func(v) for k,v in util.subdict(element, keys).items() }

        root_config = util.chain(
            transform_for_keys(self.root, ('block_size', 'page_size'), int_or_prefixed_size),
            self.root,
            {
                'block_size': int_or_prefixed_size("64B"),
                'page_size': int_or_prefixed_size("4kB")
            }
        )

        pmem = util.chain(self.pmem, {
            'name': 'DRAM', 'data_rate': 3200, 'frequency': 1600, 'channels': 1, 'ranks': 1, 'bankgroups': 8, 'banks': 4, 'bank_rows': 65536, 'bank_columns': 1024,
            'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 24, 'tRCD': 24, 'tCAS': 24, 'tRAS' : 52,
            'refresh_period': 32, 'refreshes_per_period': 8192
        })
        pmem = util.chain(pmem,(do_deprecation(pmem, pmem_deprecation_keys,pmem_deprecation_warnings)))
        
        #convert vmem boolean to string
        vmem = util.chain(
            transform_for_keys(self.vmem, ('pte_page_size',), int_or_prefixed_size),
            self.vmem,
            { 'pte_page_size': int_or_prefixed_size("4kB"), 'num_levels': 5, 'minor_fault_penalty': 200, 'randomization': 1}
        )

        # Give cores numeric indices and default cache names
        cores = [{'_index': i, **core_default_names(cpu)} for i,cpu in enumerate(self.cores)]

        path_root_names = tuple(tuple(cpu[name] for cpu in cores) for name in ('L1I', 'L1D', 'ITLB', 'DTLB'))

        # Instantiate any missing default caches
        caches = util.combine_named(self.caches.values(), ({ 'name': 'LLC' },), *map(defaults.cache_core_defaults, cores))
        ptws = util.combine_named(self.ptws.values(), *map(defaults.ptw_core_defaults, cores))

        # Remove caches that are inaccessible
        caches = filter_inaccessible(caches, itertools.chain(*path_root_names))

        # Follow paths and apply default sizings
        caches = util.combine_named(caches.values(), defaults.list_defaults(cores, caches))

        branch_parse = functools.partial(module_parse, context=branch_context)
        btb_parse = functools.partial(module_parse, context=btb_context)
        replacement_parse = functools.partial(module_parse, context=replacement_context)
        def prefetcher_parse(mod_name, cache):
            return {
                '_is_instruction_prefetcher': cache.get('_is_instruction_cache', False),
                **module_parse(mod_name, prefetcher_context)
            }

        tlb_path = itertools.chain(*(util.iter_system(caches, name) for name in itertools.chain(*path_root_names[2:])))
        data_path = itertools.chain(*(util.iter_system(caches, name) for name in itertools.chain(*path_root_names[:2])))
        caches = util.combine_named(
            # Set prefetcher_activate
            ({ 'name': k,
               'prefetch_activate': split_string_or_list(cache['prefetch_activate'])
            } for k,cache in caches.items() if 'prefetch_activate' in cache),

            # TLBs use page offsets, Caches use block offsets
            ({'name': c['name'], '_offset_bits': f'champsim::lg2({root_config["page_size"]})'} for c in tlb_path),
            ({'name': c['name'], '_offset_bits': f'champsim::lg2({root_config["block_size"]})'} for c in data_path),

            # Unfold suffixed strings
            ({'name': c['name'], **transform_for_keys(c, ('size',), int_or_prefixed_size)} for c in caches.values()),

            caches.values(),

            ## DEPRECATION
            # The listed keys are deprecated. For now, permit them but print a warning
            (do_deprecation(cache, cache_deprecation_keys) for cache in caches.values()),

            # Pass frequencies on to lower levels
            default_frequencies(cores, caches),

            # The end of the data path is the physical memory
            *((
                path_end_in(util.iter_system(caches, cpu['L1I']), 'DRAM'),
                path_end_in(util.iter_system(caches, cpu['L1D']), 'DRAM'),
                path_end_in(util.iter_system(caches, cpu['ITLB']), cpu['PTW']),
                path_end_in(util.iter_system(caches, cpu['DTLB']), cpu['PTW'])
             ) for cpu in cores),

            ({ 'name': k,
                # Mark queues that need to match full addresses on collision
               '_queue_check_full_addr': cache.get('_first_level', False) or cache.get('wq_check_full_addr', False),

                # Get module path names and unique module names
               '_replacement_data': list(map(replacement_parse, util.wrap_list(cache.get('replacement', 'lru')))),
               '_prefetcher_data': [*map(functools.partial(prefetcher_parse, cache=cache), util.wrap_list(cache.get('prefetcher', 'no')))]
            } for k,cache in caches.items())
        )

        ptws = util.combine_named(
            ptws.values(),

            ## DEPRECATION
            # The listed keys are deprecated. For now, permit them but print a warning
            (do_deprecation(ptw, ptw_deprecation_keys) for ptw in ptws.values()),

            ({ 'name': cpu['PTW'], 'frequency': cpu.get('frequency'), 'cpu': cpu.get('_index'), '_queue_factor': 32 } for cpu in cores)
        )

        cores = list(util.combine_named(cores,
            ({
                'name': c['name'],
                '_branch_predictor_data':
                    [*map(branch_parse, util.wrap_list(c.get('branch_predictor', 'hashed_perceptron')))],
                '_btb_data':
                    [*map(btb_parse, util.wrap_list(c.get('btb', 'basic_btb')))]
             } for c in cores),
            ).values()
        )

        elements = {
            'cores': cores,
            'caches': tuple(caches.values()),
            'ptws': tuple(ptws.values()),
            'pmem': pmem,
            'vmem': vmem
        }
        module_info = {
            'repl': util.combine_named(*(c['_replacement_data'] for c in caches.values()), replacement_context.find_all()),
            'pref': util.combine_named(*(c['_prefetcher_data'] for c in caches.values()), prefetcher_context.find_all()),
            'branch': util.combine_named(*(c['_branch_predictor_data'] for c in cores), branch_context.find_all()),
            'btb': util.combine_named(*(c['_btb_data'] for c in cores), btb_context.find_all())
        }

        config_extern = {
            **util.subdict(root_config, ('block_size', 'page_size', 'heartbeat_frequency')),
            'num_cores': len(cores)
        }

        return elements, module_info, config_extern

def parse_config(*configs, module_dir=None, branch_dir=None, btb_dir=None, pref_dir=None, repl_dir=None, compile_all_modules=False, verbose=False): # pylint: disable=line-too-long,
    '''
    This is the main parsing dispatch function. Programmatic use of the configuration system should use this as an entry point.

    :param configs: The configurations given here will be joined into a single configuration, then parsed. These configurations may be simply the result of parsing a JSON file, although the root should be a JSON object.
    :param module_dir: A directory to search for all modules. The structure is assumed to follow the same as the ChampSim repository: branch direction predictors are under `branch/`, replacement policies under `replacement/`, etc.
    :param branch_dir: A directory to search for branch direction predictors
    :param btb_dir: A directory to search for branch target predictors
    :param pref_dir: A directory to search for prefetchers
    :param repl_dir: A directory to search for replacement policies
    :param compile_all_modules: If true, all modules in the given directories will be compiled. If false, only the module in the configuration will be compiled.
    :param verbose: Print extra verbose output
    '''
    def list_dirs(dirname, var):
        return [
            *(os.path.join(m,dirname) for m in (module_dir or [])),
            *var,
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), dirname) # champsim root
        ]

    def do_merge(lhs, rhs):
        lhs.merge(rhs)
        return lhs
    merged_config = functools.reduce(do_merge, (NormalizedConfiguration(c, verbose=verbose) for c in configs))

    contexts = dict(
        branch_context = modules.ModuleSearchContext(list_dirs('branch', branch_dir or []), verbose=verbose),
        btb_context = modules.ModuleSearchContext(list_dirs('btb', btb_dir or []), verbose=verbose),
        replacement_context = modules.ModuleSearchContext(list_dirs('replacement', repl_dir or []), verbose=verbose),
        prefetcher_context = modules.ModuleSearchContext(list_dirs('prefetcher', pref_dir or []), verbose=verbose)
    )
    if verbose:
        for k,v in contexts.items():
            print(k, v.paths)
    elements, module_info, config_file = merged_config.apply_defaults_in(**contexts, verbose=verbose)

    if compile_all_modules:
        modules_to_compile = [*set(itertools.chain(*(d.keys() for d in module_info.values())))]
    else:
        modules_to_compile = [*set(d['name'] for d in itertools.chain(
            *(c['_replacement_data'] for c in elements['caches']),
            *(c['_prefetcher_data'] for c in elements['caches']),
            *(c['_branch_predictor_data'] for c in elements['cores']),
            *(c['_btb_data'] for c in elements['cores'])
        ))]

    return executable_name(*configs), elements, modules_to_compile, module_info, config_file
