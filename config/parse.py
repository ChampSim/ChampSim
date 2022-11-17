import itertools
import operator
import math

from . import defaults
from . import instantiation_file
from . import constants_file
from . import modules
from . import util

default_root = { 'block_size': 64, 'page_size': 4096, 'heartbeat_frequency': 10000000, 'num_cores': 1 }
default_core = { 'frequency' : 4000, 'ifetch_buffer_size': 64, 'decode_buffer_size': 32, 'dispatch_buffer_size': 32, 'rob_size': 352, 'lq_size': 128, 'sq_size': 72, 'fetch_width' : 6, 'decode_width' : 6, 'dispatch_width' : 6, 'execute_width' : 4, 'lq_width' : 2, 'sq_width' : 2, 'retire_width' : 5, 'mispredict_penalty' : 1, 'scheduler_size' : 128, 'decode_latency' : 1, 'dispatch_latency' : 1, 'schedule_latency' : 0, 'execute_latency' : 0, 'branch_predictor': 'bimodal', 'btb': 'basic_btb' }
default_dib  = { 'window_size': 16,'sets': 32, 'ways': 8 }
default_pmem = { 'name': 'DRAM', 'frequency': 3200, 'channels': 1, 'ranks': 1, 'banks': 8, 'rows': 65536, 'columns': 128, 'lines_per_column': 8, 'channel_width': 8, 'wq_size': 64, 'rq_size': 64, 'tRP': 12.5, 'tRCD': 12.5, 'tCAS': 12.5, 'turn_around_time': 7.5 }
default_vmem = { 'size': 8589934592, 'num_levels': 5, 'minor_fault_penalty': 200 }

# Assign defaults that are unique per core
def upper_levels_for(system, names):
    upper_levels = sorted(system, key=operator.itemgetter('lower_level'))
    upper_levels = itertools.groupby(upper_levels, key=operator.itemgetter('lower_level'))
    yield from ((k,v) for k,v in upper_levels if k in names)

# Scale frequencies
def scale_frequencies(it):
    it_a, it_b = itertools.tee(it, 2)
    max_freq = max(x['frequency'] for x in it_a)
    for x in it_b:
        x['frequency'] = max_freq / x['frequency']

def merge_names(x,y):
    if x is None:
        return y
    if y is None:
        return x
    return x + '_' + y

def parse_config(*configs, branch_dir=[], btb_dir=[], pref_dir=[], repl_dir=[]):
    config_file = util.chain(*configs, default_root, merge_funcs={'name': merge_names})

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
            config_file.get('cache', []),
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
            '_replacement_modpaths': [modules.default_dir(['replacement', *repl_dir], f) for f in util.wrap_list(c.get('replacement', []))],
            '_prefetcher_modpaths':  [modules.default_dir(['prefetcher', *pref_dir], f) for f in util.wrap_list(c.get('prefetcher', []))],
            '_replacement_modnames': [modules.get_module_name(modules.default_dir(['replacement', *repl_dir], f)) for f in util.wrap_list(c.get('replacement', []))],
            '_prefetcher_modnames':  [modules.get_module_name(modules.default_dir(['prefetcher', *pref_dir], f)) for f in util.wrap_list(c.get('prefetcher', []))]
            } for c in caches.values()))

    cores = list(util.combine_named(cores, ({
            'name': c['name'],
            '_branch_predictor_modpaths': [modules.default_dir(['branch', *branch_dir], f) for f in util.wrap_list(c.get('branch_predictor', []))],
            '_btb_modpaths':  [modules.default_dir(['btb', *btb_dir], f) for f in util.wrap_list(c.get('btb', []))],
            '_branch_predictor_modnames': [modules.get_module_name(modules.default_dir(['branch', *branch_dir], f)) for f in util.wrap_list(c.get('branch_predictor', []))],
            '_btb_modnames':  [modules.get_module_name(modules.default_dir(['btb', *btb_dir], f)) for f in util.wrap_list(c.get('btb', []))]
            } for c in cores)).values())

    repl_data   = modules.get_module_data('_replacement_modnames', '_replacement_modpaths', caches.values(), ['replacement', *repl_dir], modules.get_repl_data);
    pref_data   = modules.get_module_data('_prefetcher_modnames', '_prefetcher_modpaths', caches.values(), ['prefetcher', *pref_dir], modules.get_pref_data);
    branch_data = modules.get_module_data('_branch_predictor_modnames', '_branch_predictor_modpaths', cores, ['branch', *branch_dir], modules.get_branch_data);
    btb_data    = modules.get_module_data('_btb_modnames', '_btb_modpaths', cores, ['btb', *btb_dir], modules.get_btb_data);

    module_info = dict(itertools.chain(repl_data.items(), pref_data.items(), branch_data.items(), btb_data.items()))

    inst_file = instantiation_file.get_instantiation_string(cores, caches.values(), ptws.values(), pmem, vmem) # Instantiation file
    core_mods = modules.get_branch_string(branch_data) + modules.get_btb_string(btb_data)                      # Core modules file
    cache_mods = modules.get_repl_string(repl_data) + modules.get_pref_string(pref_data)                       # Cache modules file
    const_file = constants_file.get_constants_file(config_file, pmem)                                          # Constants header

    name = config_file.get('name')
    executable = config_file.get('executable_name', 'champsim' + ('' if name is None else '_'+name))

    env_vars = ('CC', 'CXX', 'CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')
    env = dict(filter(lambda kv: kv[0] in env_vars, config_file.items()))

    return executable, inst_file, core_mods, cache_mods, const_file, module_info, env

