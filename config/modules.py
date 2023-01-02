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

def data_getter(prefix, module_name, funcs):
    return {
        'opts': { 'CXXFLAGS': ('-Wno-unused-parameter',) },
        'func_map': { k: '_'.join((prefix, module_name, k)) for k in funcs } # Resolve function names
    }

def get_branch_data(module_name):
    return data_getter('bpred', module_name, ('initialize_branch_predictor', 'last_branch_result', 'predict_branch'))

def get_btb_data(module_name):
    return data_getter('btb', module_name, ('initialize_btb', 'update_btb', 'btb_prediction'))

def get_pref_data(module_name, is_instruction_cache=False):
    return data_getter('ipref' if is_instruction_cache else 'pref', module_name, ('prefetcher_initialize', 'prefetcher_cache_operate', 'prefetcher_branch_operate', 'prefetcher_cache_fill', 'prefetcher_cycle_operate', 'prefetcher_final_stats'))

def get_repl_data(module_name):
    return data_getter('repl', module_name, ('initialize_replacement', 'find_victim', 'update_replacement_state', 'replacement_final_stats'))

def get_module_variant(fname, rtype, join_op, args, varname, keynamelist, fnamelist):
    joiner = '' if rtype == 'void' else 'result ' + join_op + ' '
    nodiscard = '' if rtype == 'void' else '[[nodiscard]] '

    # Declare name-mangled functions
    yield from (('{}{} {}({});').format(nodiscard, rtype, name, ', '.join(a[0] for a in args)) for name in fnamelist)

    # Define discriminator function
    yield from ('{}{} impl_{}({})'.format(nodiscard, rtype, fname, ', '.join(a[0]+' '+a[1] for a in args)), '{')

    # If the function expects a result, declare it
    if rtype != 'void':
        yield '  ' + rtype + ' result{};'

    # Discriminate between the module variants
    yield from (('  if ({}[champsim::lg2({})]) {}{}({});').format(varname, k, joiner, n, ', '.join(a[1] for a in args)) for k,n in zip(keynamelist, fnamelist))

    # Return result
    if rtype != 'void':
        yield '  return result;'

    yield '}'
    yield ''

def get_all_variant_lines(prefix, num_varname, varname, mod_data, variant_data):
    yield f'constexpr static std::size_t {num_varname} = {len(mod_data)};'
    yield from ('constexpr static unsigned long long {0}{2:{prec}} = 1ull << {1};'.format(prefix, *x, prec=max(len(k) for k in mod_data)) for x in enumerate(mod_data))

    kv = {k: [(prefix+name, v['func_map'][k]) for name,v in mod_data.items()] for k in variant_data.keys()}
    yield ''
    yield from itertools.chain.from_iterable(get_module_variant(funcargs[0], *funcargs[1], varname, *zip(*kv[funcargs[0]])) for funcargs in variant_data.items())

def get_branch_lines(branch_data):
    branch_variant_data = {
        'initialize_branch_predictor': ('void', '', tuple()),
        'last_branch_result': ('void', '', (('champsim::address', 'ip'), ('champsim::address', 'target'), ('bool', 'taken'), ('uint8_t', 'branch_type'))),
        'predict_branch': ('bool', '|=', (('champsim::address','ip'),))
    }

    yield from get_all_variant_lines('b', 'NUM_BRANCH_MODULES', 'bpred_type', branch_data, branch_variant_data)

def get_btb_lines(btb_data):
    btb_variant_data = {
        'initialize_btb': ('void', '', tuple()),
        'update_btb': ('void', '', (('champsim::address','ip'), ('champsim::address','predicted_target'), ('bool','taken'), ('uint8_t','branch_type'))),
        'btb_prediction': ('std::pair<champsim::address, bool>', '=', (('champsim::address','ip'),))
    }

    yield from get_all_variant_lines('t', 'NUM_BTB_MODULES', 'btb_type', btb_data, btb_variant_data)

def get_pref_lines(pref_data):
    pref_nonbranch_variant_data = {
        'prefetcher_initialize': ('void', '', tuple()),
        'prefetcher_cache_operate': ('uint32_t', '^=', (('champsim::address', 'addr'), ('champsim::address', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in'))),
        'prefetcher_cache_fill': ('uint32_t', '^=', (('champsim::address', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('champsim::address', 'evicted_addr'), ('uint32_t', 'metadata_in'))),
        'prefetcher_cycle_operate': ('void', '', tuple()),
        'prefetcher_final_stats': ('void', '', tuple())
    }

    pref_branch_variant_data = {
            'prefetcher_branch_operate': ('void', '', (('champsim::address', 'ip'), ('uint8_t', 'branch_type'), ('champsim::address', 'branch_target')))
    }

    yield from get_all_variant_lines('p', 'NUM_PREFETCH_MODULES', 'pref_type', pref_data, pref_nonbranch_variant_data)
    yield '// Assert data prefetchers do not operate on branches'
    yield from ('void {}(champsim::address, uint8_t, champsim::address) {{ assert(false); }}'.format(p['func_map']['prefetcher_branch_operate']) for p in pref_data.values() if not p.get('_is_instruction_prefetcher'))
    yield from (get_module_variant('prefetcher_branch_operate', *pref_branch_variant_data['prefetcher_branch_operate'], 'pref_type', *zip(*(('p'+kv[0],kv[1]['func_map']['prefetcher_branch_operate']) for kv in pref_data.items() if kv[1].get('_is_instruction_prefetcher')))))

def get_repl_lines(repl_data):
    repl_variant_data = {
        'initialize_replacement': ('void', '', tuple()),
        'find_victim': ('uint32_t', '=', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const BLOCK*','current_set'), ('champsim::address','ip'), ('champsim::address','full_addr'), ('uint32_t','type'))),
        'update_replacement_state': ('void', '', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('champsim::address','full_addr'), ('champsim::address','ip'), ('champsim::address','victim_addr'), ('uint32_t','type'), ('uint8_t','hit'))),
        'replacement_final_stats': ('void', '', tuple())
    }

    yield from get_all_variant_lines('r', 'NUM_REPLACEMENT_MODULES', 'repl_type', repl_data, repl_variant_data)
