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

def get_module_variant(fname_key, impl_fname, rtype, join_op, args, varname, keyprefix, pref_data):
    retval = ''

    if rtype != 'void':
        joiner = 'result ' + join_op + ' '
    else:
        joiner = ''

    # Declare name-mangled functions
    retval += '\n'.join(('{} {'+fname_key+'}({});').format(rtype, ', '.join(a[0] for a in args), **p) for p in pref_data.values())

    # Define discriminator function
    retval += '\n{} {}({})\n{{\n'.format(rtype, impl_fname, ', '.join(a[0]+' '+a[1] for a in args))

    # If the function expects a result, declare it
    if rtype != 'void':
        retval += '  ' + rtype + ' result{};\n'

    # Discriminate between the module variants
    retval += '\n'.join(('  if ({}[champsim::lg2({}{})]) {}{'+fname_key+'}({});').format(varname, keyprefix, k, joiner, ', '.join(a[1] for a in args), **p) for k,p in pref_data.items())
    retval += '\n'

    # Return result
    if rtype != 'void':
        retval += '  return result;\n'

    retval += '}\n'
    retval += '\n'

    return retval

def get_branch_data(module_name):
    retval = {}

    # Resolve branch predictor function names
    retval['bpred_initialize'] = 'bpred_' + module_name + '_initialize'
    retval['bpred_last_result'] = 'bpred_' + module_name + '_last_result'
    retval['bpred_predict'] = 'bpred_' + module_name + '_predict'

    retval['opts'] = { 'CXXFLAGS': ('-Wno-unused-parameter',) }

    retval['func_map'] = {
        'initialize_branch_predictor': retval['bpred_initialize'],
        'last_branch_result': retval['bpred_last_result'],
        'predict_branch': retval['bpred_predict']
    }

    return retval

branch_variant_data = {
        'bpred_initialize': ('impl_branch_predictor_initialize', 'void', '', tuple()),
        'bpred_last_result': ('impl_last_branch_result', 'void', '', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type'))),
        'bpred_predict': ('impl_predict_branch', 'uint8_t', '|=', (('uint64_t','ip'),))
        }

def get_branch_string(branch_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_BRANCH_MODULES = {len(branch_data)};\n'

    retval += '\n'.join('constexpr static unsigned long long b{1:{prec}} = 1ull << {0};'.format(*x, prec=max(len(k) for k in branch_data)) for x in enumerate(branch_data))
    retval += '\n\n'

    retval += get_module_variant('bpred_initialize', *branch_variant_data['bpred_initialize'], 'bpred_type', 'b', branch_data)
    retval += get_module_variant('bpred_last_result', *branch_variant_data['bpred_last_result'], 'bpred_type', 'b', branch_data)
    retval += get_module_variant('bpred_predict', *branch_variant_data['bpred_predict'], 'bpred_type', 'b', branch_data)

    return retval

def get_btb_data(module_name):
    retval = {}

    # Resolve BTB function names
    retval['btb_initialize'] = 'btb_' + module_name + '_initialize'
    retval['btb_update'] = 'btb_' + module_name + '_update'
    retval['btb_predict'] = 'btb_' + module_name + '_predict'

    retval['opts'] = { 'CXXFLAGS': ('-Wno-unused-parameter',) }

    retval['func_map'] = {
        'initialize_btb': retval['btb_initialize'],
        'update_btb': retval['btb_update'],
        'btb_prediction': retval['btb_predict']
    }

    return retval

btb_variant_data = {
        'btb_initialize': ('impl_btb_initialize', 'void', '', tuple()),
        'btb_update': ('impl_update_btb', 'void', '', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type'))),
        'btb_predict': ('impl_btb_prediction', 'std::pair<uint64_t, uint8_t>', '=', (('uint64_t','ip'),))
        }

def get_btb_string(btb_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_BTB_MODULES = {len(btb_data)};\n'

    retval += '\n'.join('constexpr static unsigned long long t{1:{prec}} = 1ull << {0};'.format(*x, prec=max(len(k) for k in btb_data)) for x in enumerate(btb_data))
    retval += '\n\n'

    retval += get_module_variant('btb_initialize', *btb_variant_data['btb_initialize'], 'btb_type', 't', btb_data)
    retval += get_module_variant('btb_update', *btb_variant_data['btb_update'], 'btb_type', 't', btb_data)
    retval += get_module_variant('btb_predict', *btb_variant_data['btb_predict'], 'btb_type', 't', btb_data)

    return retval

def get_pref_data(module_name, is_instruction_cache=False):
    retval = {}

    prefix = 'ipref_' if is_instruction_cache else 'pref_'
    # Resolve prefetcher function names
    retval['prefetcher_initialize'] = prefix + module_name + '_initialize'
    retval['prefetcher_cache_operate'] = prefix + module_name + '_cache_operate'
    retval['prefetcher_branch_operate'] = prefix + module_name + '_branch_operate'
    retval['prefetcher_cache_fill'] = prefix + module_name + '_cache_fill'
    retval['prefetcher_cycle_operate'] = prefix + module_name + '_cycle_operate'
    retval['prefetcher_final_stats'] = prefix + module_name + '_final_stats'

    retval['opts'] = { 'CXXFLAGS': ('-Wno-unused-parameter',) }

    retval['func_map'] = {
        'prefetcher_initialize': retval['prefetcher_initialize'],
        'prefetcher_cache_operate': retval['prefetcher_cache_operate'],
        'prefetcher_branch_operate': retval['prefetcher_branch_operate'],
        'prefetcher_cache_fill': retval['prefetcher_cache_fill'],
        'prefetcher_cycle_operate': retval['prefetcher_cycle_operate'],
        'prefetcher_final_stats': retval['prefetcher_final_stats'],
    }

    return retval

pref_variant_args = {
    'prefetcher_initialize': ('impl_prefetcher_initialize', 'void', '', tuple()),
    'prefetcher_branch_operate': ('impl_prefetcher_branch_operate', 'void', '', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target'))),
    'prefetcher_cache_operate': ('impl_prefetcher_cache_operate', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in'))),
    'prefetcher_cache_fill': ('impl_prefetcher_cache_fill', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in'))),
    'prefetcher_cycle_operate': ('impl_prefetcher_cycle_operate', 'void', '', tuple()),
    'prefetcher_final_stats': ('impl_prefetcher_final_stats', 'void', '', tuple())
    }

def get_pref_string(pref_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_PREFETCH_MODULES = {len(pref_data)};\n'

    retval += '\n'.join('constexpr static unsigned long long p{1:{prec}} = 1ull << {0};'.format(*x, prec=max(len(k) for k in pref_data)) for x in enumerate(pref_data))
    retval += '\n\n'

    retval += get_module_variant('prefetcher_initialize', *pref_variant_args['prefetcher_initialize'], 'pref_type', 'p', pref_data)
    retval += get_module_variant('prefetcher_cache_operate', *pref_variant_args['prefetcher_cache_operate'], 'pref_type', 'p', pref_data)
    retval += get_module_variant('prefetcher_cache_fill', *pref_variant_args['prefetcher_cache_fill'], 'pref_type', 'p', pref_data)
    retval += get_module_variant('prefetcher_cycle_operate', *pref_variant_args['prefetcher_cycle_operate'], 'pref_type', 'p', pref_data)
    retval += get_module_variant('prefetcher_final_stats', *pref_variant_args['prefetcher_final_stats'], 'pref_type', 'p', pref_data)

    retval += '// Assert data prefetchers do not operate on branches\n'
    retval += '\n'.join('void {prefetcher_branch_operate}(uint64_t, uint8_t, uint64_t) {{ assert(false); }}'.format(**p) for p in pref_data.values() if not p.get('_is_instruction_prefetcher'))
    retval += '\n'
    retval += get_module_variant('prefetcher_branch_operate', *pref_variant_args['prefetcher_branch_operate'], 'pref_type', 'p', {k:v for k,v in pref_data.items() if v.get('_is_instruction_prefetcher')})

    return retval;

def get_repl_data(module_name):
    retval = {}

    # Resolve cache replacment function names
    retval['init_func_name'] = 'repl_' + module_name + '_initialize'
    retval['find_victim_func_name'] = 'repl_' + module_name + '_victim'
    retval['update_func_name'] = 'repl_' + module_name + '_update'
    retval['final_func_name'] = 'repl_' + module_name + '_final_stats'

    retval['opts'] = { 'CXXFLAGS': ('-Wno-unused-parameter',) }

    retval['func_map'] = {
        'initialize_replacement': retval['init_func_name'],
        'find_victim': retval['find_victim_func_name'],
        'update_replacement_state': retval['update_func_name'],
        'replacement_final_stats': retval['final_func_name']
    }

    return retval

repl_variant_data = {
        'init_func_name': ('impl_replacement_initialize', 'void', '', tuple()),
        'find_victim_func_name': ('impl_replacement_find_victim', 'uint32_t', '=', (('uint32_t','cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type'))),
        'update_func_name': ('impl_replacement_update_state', 'void', '', (('uint32_t','cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit'))),
        'final_func_name': ('impl_replacement_final_stats', 'void', '', tuple())
        }

def get_repl_string(repl_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_REPLACEMENT_MODULES = {len(repl_data)};\n'

    retval += '\n'.join('constexpr static unsigned long long r{1:{prec}} = 1ull << {0};'.format(*x, prec=max(len(k) for k in repl_data)) for x in enumerate(repl_data))
    retval += '\n\n'

    retval += get_module_variant('init_func_name', *repl_variant_data['init_func_name'], 'repl_type', 'r', repl_data)
    retval += get_module_variant('find_victim_func_name', *repl_variant_data['find_victim_func_name'], 'repl_type', 'r', repl_data)
    retval += get_module_variant('update_func_name', *repl_variant_data['update_func_name'], 'repl_type', 'r', repl_data)
    retval += get_module_variant('final_func_name', *repl_variant_data['final_func_name'], 'repl_type', 'r', repl_data)

    return retval
