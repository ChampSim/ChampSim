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
        'name': module_name,
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

def mangled_declarations(rtype, names, args, attrs=[]):
    if rtype != 'void':
        local_attrs = ('nodiscard',)
    else:
        local_attrs = tuple()
    attrstring = ', '.join(itertools.chain(attrs, local_attrs))

    argstring = ', '.join(a[0] for a in args)
    yield from ('[[{}]] {} {}({});'.format(attrstring, rtype, name, argstring) for name in names)

def mangled_prohibited_definitions(rtype, names, args, attrs=[]):
    local_attrs = ('noreturn',)
    attrstring = ', '.join(itertools.chain(attrs, local_attrs))

    argstring = ', '.join(a[0] for a in args)
    yield from ('[[{}]] {} {}({}) {{ throw std::runtime_error("Not implemented"); }}'.format(attrstring, rtype, name, argstring) for name in names)

def discriminator_function_declaration(fname, rtype, args, attrs=[], classname=None):
    if rtype != 'void':
        local_attrs = ('nodiscard',)
    else:
        local_attrs = tuple()

    if classname is None:
        attrstring = '[[' + ', '.join(itertools.chain(attrs, local_attrs)) + ']]'
        argstring = ', '.join(a[0] for a in args)
    else:
        attrstring = ''
        argstring = ', '.join((a[0]+' '+a[1]) for a in args)

    funcstring = ((classname + '::') if classname is not None else '') + 'impl_' + fname
    yield '{} {} {}({}){}'.format(attrstring, rtype, funcstring, argstring, ';' if classname is None else '')

def discriminator_function_definition(fname, rtype, join_op, args, varname, zipped_keys_and_funcs):
    joiner = '' if rtype == 'void' else 'result ' + join_op + ' '

    yield '{'

    # If the function expects a result, declare it
    if rtype != 'void':
        yield '  ' + rtype + ' result{};'

    # Discriminate between the module variants
    yield from ('  if ({}[champsim::lg2({})]) {}{}({});'.format(varname, k, joiner, n, ', '.join(a[1] for a in args)) for k,n in zipped_keys_and_funcs)

    # Return result
    if rtype != 'void':
        yield '  return result;'

    yield '}'

def get_module_variant_declarations(fname, rtype, args, fnamelist, attrs=[]):
    yield from mangled_declarations(rtype, fnamelist, args, attrs=attrs)
    yield from discriminator_function_declaration(fname, rtype, args, attrs=attrs)
    yield ''

def get_discriminator(fname, rtype, join_op, args, varname, zipped_keys_and_funcs, attrs=[], classname=None):
    yield from discriminator_function_declaration(fname, rtype, args, attrs=attrs, classname=classname)
    yield from discriminator_function_definition(fname, rtype, join_op, args, varname, zipped_keys_and_funcs)
    yield ''

def constants_for_modules(prefix, num_varname, mod_data):
    yield f'constexpr static std::size_t {num_varname} = {len(mod_data)};'
    yield from ('constexpr static unsigned long long {0}{2:{prec}} = 1ull << {1};'.format(prefix, n, data['name'], prec=max(len(k['name']) for k in mod_data)) for n,data in enumerate(mod_data))

def get_branch_lines(branch_data):
    prefix = 'b'
    varname = 'bpred_type'
    varname_size_name = 'NUM_BRANCH_MODULES'
    branch_variant_data = [
        ('initialize_branch_predictor', 'void', '', tuple()),
        ('last_branch_result', 'void', '', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type'))),
        ('predict_branch', 'uint8_t', '|=', (('uint64_t','ip'),))
    ]

    return (
        itertools.chain(
            constants_for_modules(prefix, varname_size_name, branch_data.values()), ('',),

            # Declare name-mangled functions
            *(get_module_variant_declarations(fname, rtype, args, [v['func_map'][fname] for v in branch_data.values()]) for fname, rtype, _, args in branch_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, rtype, join_op, args, varname, [(prefix + v['name'], v['func_map'][fname]) for v in branch_data.values()], classname='O3_CPU') for fname, rtype, join_op, args in branch_variant_data)
        )
       )

def get_btb_lines(btb_data):
    prefix = 't'
    varname = 'btb_type'
    varname_size_name = 'NUM_BTB_MODULES'
    btb_variant_data = [
        ('initialize_btb', 'void', '', tuple()),
        ('update_btb', 'void', '', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type'))),
        ('btb_prediction', 'std::pair<uint64_t, uint8_t>', '=', (('uint64_t','ip'),))
    ]

    return (
        itertools.chain(
            constants_for_modules(prefix, varname_size_name, btb_data.values()), ('',),

            # Declare name-mangled functions
            *(get_module_variant_declarations(fname, rtype, args, [v['func_map'][fname] for v in btb_data.values()]) for fname, rtype, _, args in btb_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, rtype, join_op, args, varname, [(prefix + v['name'], v['func_map'][fname]) for v in btb_data.values()], classname='O3_CPU') for fname, rtype, join_op, args in btb_variant_data)
        )
       )

def get_pref_lines(pref_data):
    prefix = 'p'
    varname = 'pref_type'
    varname_size_name = 'NUM_PREFETCH_MODULES'

    pref_nonbranch_variant_data = [
        ('prefetcher_initialize', 'void', '', tuple()),
        ('prefetcher_cache_operate', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in'))),
        ('prefetcher_cache_fill', 'uint32_t', '^=', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in'))),
        ('prefetcher_cycle_operate', 'void', '', tuple()),
        ('prefetcher_final_stats', 'void', '', tuple())
    ]

    pref_branch_variant_data = [
        ('prefetcher_branch_operate', 'void', '', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')))
    ]

    return (
        itertools.chain(
            constants_for_modules(prefix, varname_size_name, pref_data.values()), ('',),

            # Establish functions common to all prefetchers
            *(get_module_variant_declarations(fname, rtype, args, [v['func_map'][fname] for v in pref_data.values()]) for fname, rtype, _, args in pref_nonbranch_variant_data),

            # Establish functions that only matter to instruction prefetchers
            ('', '// Assert data prefetchers do not operate on branches'),
            *(mangled_prohibited_definitions(rtype, [v['func_map'][fname] for v in pref_data.values() if not v.get('_is_instruction_prefetcher')], args) for fname, rtype, _, args in pref_branch_variant_data),
            *(get_module_variant_declarations(fname, rtype, args, [v['func_map'][fname] for v in pref_data.values() if v.get('_is_instruction_prefetcher')]) for fname, rtype, _, args in pref_branch_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, rtype, join_op, args, varname, [(prefix + v['name'], v['func_map'][fname]) for v in pref_data.values()], classname='CACHE') for fname, rtype, join_op, args in itertools.chain(pref_nonbranch_variant_data, pref_branch_variant_data))
        )
       )

def get_repl_lines(repl_data):
    prefix = 'r'
    varname = 'repl_type'
    varname_size_name = 'NUM_REPLACEMENT_MODULES'

    repl_variant_data = [
        ('initialize_replacement', 'void', '', tuple()),
        ('find_victim', 'uint32_t', '=', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type'))),
        ('update_replacement_state', 'void', '', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit'))),
        ('replacement_final_stats', 'void', '', tuple())
    ]

    return (
        itertools.chain(
            constants_for_modules(prefix, varname_size_name, repl_data.values()), ('',),

            # Declare name-mangled functions
            *(get_module_variant_declarations(fname, rtype, args, [v['func_map'][fname] for v in repl_data.values()]) for fname, rtype, _, args in repl_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, rtype, join_op, args, varname, [(prefix + v['name'], v['func_map'][fname]) for v in repl_data.values()], classname='CACHE') for fname, rtype, join_op, args in repl_variant_data)
        )
       )
