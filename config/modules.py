import os
import itertools

from . import util

def get_module_name(path):
    fname_translation_table = str.maketrans('./-','_DH')
    return path.translate(fname_translation_table)

def norm_dirname(f):
    return os.path.relpath(os.path.expandvars(os.path.expanduser(f)))

# Get the paths to built-in modules
def default_modules(dirname):
    files = (os.path.join(dirname, d) for d in os.listdir(dirname))
    files = filter(os.path.isdir, files)
    yield from ({'name': get_module_name(f), 'fname': f, '_is_instruction_prefetcher': f.endswith('_instr')} for f in files)

# Try the built-in module directories, then try to interpret as a path
def default_dir(dirnames, f):
    return next(filter(os.path.exists, map(norm_dirname, itertools.chain(
        (os.path.join(dirname, f) for dirname in dirnames), # Prepend search paths
        (f,) # Interpret as file path
    ))))

def get_module_data(names_key, paths_key, values, directories, get_func):
    namekey_pairs = itertools.chain(*(zip(c[names_key], c[paths_key], itertools.repeat(c.get('_is_instruction_prefetcher', False))) for c in values))
    data = util.combine_named(
        itertools.chain(*(default_modules(directory) for directory in directories if os.path.exists(directory))),
        ({'name': name, 'fname': path, '_is_instruction_prefetcher': is_instr} for name,path,is_instr in namekey_pairs)
        )
    return {k: util.chain((get_func(k,v['_is_instruction_prefetcher']) if v['_is_instruction_prefetcher'] else get_func(k)), v) for k,v in data.items()}

def get_branch_data(module_name):
    return {
        'opts': { 'CXXFLAGS': ('-Wno-unused-parameter',) },

        # Resolve branch predictor function names
        'func_map': {
            'initialize_branch_predictor': 'bpred_' + module_name + '_initialize',
            'last_branch_result': 'bpred_' + module_name + '_last_result',
            'predict_branch': 'bpred_' + module_name + '_predict'
        }
    }

def get_btb_data(module_name):
    return {
        'opts': { 'CXXFLAGS': ('-Wno-unused-parameter',) },

        # Resolve BTB function names
        'func_map': {
            'initialize_btb': 'btb_' + module_name + '_initialize',
            'update_btb': 'btb_' + module_name + '_update',
            'btb_prediction': 'btb_' + module_name + '_predict',
        },
    }

def get_pref_data(module_name, is_instruction_cache=False):
    prefix = 'ipref_' if is_instruction_cache else 'pref_'

    return {
        'opts': { 'CXXFLAGS': ('-Wno-unused-parameter',) },

        # Resolve prefetcher function names
        'func_map': {
            'prefetcher_initialize': prefix + module_name + '_initialize',
            'prefetcher_cache_operate': prefix + module_name + '_cache_operate',
            'prefetcher_branch_operate': prefix + module_name + '_branch_operate',
            'prefetcher_cache_fill': prefix + module_name + '_cache_fill',
            'prefetcher_cycle_operate': prefix + module_name + '_cycle_operate',
            'prefetcher_final_stats': prefix + module_name + '_final_stats',
        }
    }

def get_repl_data(module_name):
    return {
        'opts': { 'CXXFLAGS': ('-Wno-unused-parameter',) },

        # Resolve cache replacment function names
        'func_map': {
            'initialize_replacement': 'repl_' + module_name + '_initialize',
            'find_victim': 'repl_' + module_name + '_victim',
            'update_replacement_state': 'repl_' + module_name + '_update',
            'replacement_final_stats': 'repl_' + module_name + '_final_stats',
        }
    }

def get_module_variant(impl_fname, rtype, join_op, args, varname, keynamelist, fnamelist):
    if rtype != 'void':
        joiner = 'result ' + join_op + ' '
    else:
        joiner = ''

    # Declare name-mangled functions
    yield from (('{} {}({});').format(rtype, name, ', '.join(a[0] for a in args)) for name in fnamelist)

    # Define discriminator function
    yield from ('', '{} {}({})'.format(rtype, impl_fname, ', '.join(a[0]+' '+a[1] for a in args)), '{')

    # If the function expects a result, declare it
    if rtype != 'void':
        yield '  ' + rtype + ' result{};'

    # Discriminate between the module variants
    yield from (('  if ({}[champsim::lg2({})]) {}{}({});').format(varname, k, joiner, n, ', '.join(a[1] for a in args)) for k,n in zip(keynamelist, fnamelist))

    # Return result
    if rtype != 'void':
        yield '  return result;'

    yield '}'

def get_all_variant_lines(prefix, num_varname, varname, mod_data, variant_data):
    yield f'constexpr static std::size_t {num_varname} = {len(mod_data)};'
    yield from ('constexpr static unsigned long long {0}{2:{prec}} = 1ull << {1};'.format(prefix, *x, prec=max(len(k) for k in mod_data)) for x in enumerate(mod_data))

    kv = {k: [(prefix+name, v['func_map'][k]) for name,v in mod_data.items()] for k in variant_data.keys()}
    yield ''
    yield from itertools.chain.from_iterable(get_module_variant(*funcargs[1], varname, *zip(*kv[funcargs[0]])) for funcargs in variant_data.items())

def get_branch_lines(branch_data):
    branch_variant_data = {
        'initialize_branch_predictor': ('impl_branch_predictor_initialize', 'void', '', tuple()),
        'last_branch_result': ('impl_last_branch_result', 'void', '', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type'))),
        'predict_branch': ('impl_predict_branch', 'uint8_t', '|=', (('uint64_t','ip'),))
    }

    yield from get_all_variant_lines('b', 'NUM_BRANCH_MODULES', 'bpred_type', branch_data, branch_variant_data)

def get_btb_lines(btb_data):
    btb_variant_data = {
        'initialize_btb': ('impl_btb_initialize', 'void', '', tuple()),
        'update_btb': ('impl_update_btb', 'void', '', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type'))),
        'btb_prediction': ('impl_btb_prediction', 'std::pair<uint64_t, uint8_t>', '=', (('uint64_t','ip'),))
    }

    yield from get_all_variant_lines('t', 'NUM_BTB_MODULES', 'btb_type', btb_data, btb_variant_data)

def get_pref_lines(pref_data):
    pref_nonbranch_variant_data = {
        'prefetcher_initialize': ('impl_prefetcher_initialize', 'void', '', tuple()),
        'prefetcher_cache_operate': ('impl_prefetcher_cache_operate', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in'))),
        'prefetcher_cache_fill': ('impl_prefetcher_cache_fill', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in'))),
        'prefetcher_cycle_operate': ('impl_prefetcher_cycle_operate', 'void', '', tuple()),
        'prefetcher_final_stats': ('impl_prefetcher_final_stats', 'void', '', tuple())
    }

    pref_branch_variant_data = {
        'prefetcher_branch_operate': ('impl_prefetcher_branch_operate', 'void', '', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')))
    }

    yield from get_all_variant_lines('p', 'NUM_PREFETCH_MODULES', 'pref_type', pref_data, pref_nonbranch_variant_data)
    yield '// Assert data prefetchers do not operate on branches'
    yield from ('void {}(uint64_t, uint8_t, uint64_t) {{ assert(false); }}'.format(p['func_map']['prefetcher_branch_operate']) for p in pref_data.values() if not p.get('_is_instruction_prefetcher'))
    yield from (get_module_variant(*pref_branch_variant_data['prefetcher_branch_operate'], 'pref_type', *zip(*(('p'+kv[0],kv[1]['func_map']['prefetcher_branch_operate']) for kv in pref_data.items() if kv[1].get('_is_instruction_prefetcher')))))

def get_repl_lines(repl_data):
    repl_variant_data = {
        'initialize_replacement': ('impl_replacement_initialize', 'void', '', tuple()),
        'find_victim': ('impl_replacement_find_victim', 'uint32_t', '=', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type'))),
        'update_replacement_state': ('impl_replacement_update_state', 'void', '', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit'))),
        'replacement_final_stats': ('impl_replacement_final_stats', 'void', '', tuple())
    }

    yield from get_all_variant_lines('r', 'NUM_REPLACEMENT_MODULES', 'repl_type', repl_data, repl_variant_data)
