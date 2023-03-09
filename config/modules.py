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

# Utility function to get a mangled module name from the path to its sources
def get_module_name(path, start=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))):
    fname_translation_table = str.maketrans('./-','_DH')
    return os.path.relpath(path, start=start).translate(fname_translation_table)

class ModuleSearchContext:
    def __init__(self, paths):
        self.paths = [p for p in paths if os.path.exists(p) and os.path.isdir(p)]

    def data_from_path(self, path):
        return {'name': get_module_name(path), 'fname': path, '_is_instruction_prefetcher': path.endswith('_instr')}

    # Try the context's module directories, then try to interpret as a path
    def find(self, module):
        # Return a normalized directory: variables and user shorthands are expanded
        path = os.path.relpath(os.path.expandvars(os.path.expanduser(next(filter(os.path.exists, itertools.chain(
            (os.path.join(dirname, module) for dirname in self.paths), # Prepend search paths
            (module,) # Interpret as file path
        ))))))

        return self.data_from_path(path)

    def find_all(self):
        base_dirs = [next(os.walk(p)) for p in self.paths]
        files = itertools.starmap(os.path.join, itertools.chain(*(zip(itertools.repeat(b), d) for b,d,_ in base_dirs)))
        return [self.data_from_path(f) for f in files]

# A unifying function for the four module types to return their information
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

# Generate C++ code giving the mangled module specialization functions
def mangled_declarations(rtype, names, args, attrs=[]):
    if rtype != 'void':
        local_attrs = ('nodiscard',)
    else:
        local_attrs = tuple()
    attrstring = ', '.join(itertools.chain(attrs, local_attrs))

    argstring = ', '.join(a[0] for a in args)
    yield from ('[[{}]] {} {}({});'.format(attrstring, rtype, name, argstring) for name in names)

# Generate C++ code giving the mangled module specialization functions that are not implemented
def mangled_prohibited_definitions(fname, names, args=tuple(), rtype='void', *tail, attrs=[]):
    local_attrs = ('noreturn',)
    attrstring = ', '.join(itertools.chain(attrs, local_attrs))

    argstring = ', '.join(a[0] for a in args)
    yield from ('[[{}]] {} {}({}) {{ throw std::runtime_error("Not implemented"); }}'.format(attrstring, rtype, name, argstring) for name in names)

# Generate C++ code giving the declaration for a discriminator function. If the class name is given, the declaration is assumed to be outside the class declaration
def discriminator_function_declaration(fname, rtype, args, varname, secondary_varname, classname):
    yield 'template <unsigned long long {}, unsigned long long {}>'.format(*sorted([varname, secondary_varname]))
    argstring = ', '.join((a[0]+' '+a[1]) for a in args)
    yield '{} {}::impl_{}({})'.format(rtype, classname, fname, argstring)

# Generate C++ code for the body of a discriminator function that returns void
def discriminator_function_definition_void(fname, args, varname, zipped_keys_and_funcs, classname):
    # Discriminate between the module variants
    yield from ('  if constexpr (({} & {}::{}) != 0) intern_->{}({});'.format(varname, classname, k, n, ', '.join(a[1] for a in args)) for k,n in zipped_keys_and_funcs)

# Generate C++ code for the body of a discriminator function that returns nonvoid
def discriminator_function_definition_nonvoid(fname, rtype, join_op, args, varname, zipped_keys_and_funcs, classname):
    # Declare result
    yield '  ' + rtype + ' result{};'
    yield '  ' + join_op + '<decltype(result)> joiner{};'

    # Discriminate between the module variants
    yield from ('  if constexpr (({} & {}::{}) != 0) result = joiner(result, intern_->{}({}));'.format(varname, classname, k, n, ', '.join(a[1] for a in args)) for k,n in zipped_keys_and_funcs)

    # Return result
    yield '  return result;'

# Generate C++ code for the body of a discriminator function
def discriminator_function_definition(fname, rtype, join_op, args, varname, zipped_keys_and_funcs, classname):
    yield '{'

    if rtype == 'void':
        yield from discriminator_function_definition_void(fname, args, varname, zipped_keys_and_funcs, classname)
    else:
        yield from discriminator_function_definition_nonvoid(fname, rtype, join_op, args, varname, zipped_keys_and_funcs, classname)

    yield '}'

# For a given module function, generate C++ code declaring its mangled specialization declarations and a discriminator function
def get_module_variant_declarations(fname, fnamelist, args=tuple(), rtype='void', *tail, attrs=[]):
    yield from mangled_declarations(rtype, fnamelist, args, attrs=attrs)
    yield ''

# For a given module function, generate C++ code defining the discriminator function
def get_discriminator(fname, varname, secondary_varname, zipped_keys_and_funcs, args=tuple(), rtype='void', join_op=None, *tail, classname=None):
    yield from discriminator_function_declaration(fname, rtype, args, varname, secondary_varname, classname)
    yield from discriminator_function_definition(fname, rtype, join_op, args, varname, zipped_keys_and_funcs, classname.split(':')[0])
    yield ''

# For a set of module data, generate C++ code defining the constants that distinguish the modules
def constants_for_modules(prefix, mod_data):
    yield from ('constexpr static unsigned long long {0}{2:{prec}} = 1ull << {1};'.format(prefix, n, data['name'], prec=max(len(k['name']) for k in mod_data)) for n,data in enumerate(mod_data))

# Return a pair containing two generators: The first generates C++ code declaring all functions for the O3_CPU modules, and the second generates C++ code defining the functions
def get_ooo_cpu_module_lines(branch_data, btb_data):
    branch_prefix = 'b'
    branch_varname = 'B_FLAG'
    branch_variant_data = [
        ('initialize_branch_predictor',),
        ('last_branch_result', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type'))),
        ('predict_branch', (('uint64_t','ip'),), 'uint8_t', 'std::bit_or')
    ]

    btb_prefix = 't'
    btb_varname = 'T_FLAG'
    btb_variant_data = [
        ('initialize_btb',),
        ('update_btb', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type'))),
        ('btb_prediction', (('uint64_t','ip'),), 'std::pair<uint64_t, uint8_t>', 'champsim::detail::take_last')
    ]

    classname = 'O3_CPU::module_model<' + branch_varname + ', ' + btb_varname + '>'

    return (
        itertools.chain(
            constants_for_modules(branch_prefix, branch_data.values()), ('',),
            constants_for_modules(btb_prefix, btb_data.values()), ('',),

            # Declare name-mangled functions
            *(get_module_variant_declarations(fname, [v['func_map'][fname] for v in branch_data.values()], *finfo) for fname, *finfo in branch_variant_data),
            *(get_module_variant_declarations(fname, [v['func_map'][fname] for v in btb_data.values()], *finfo) for fname, *finfo in btb_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, branch_varname, btb_varname, [(branch_prefix + v['name'], v['func_map'][fname]) for v in branch_data.values()], *finfo, classname=classname) for fname, *finfo in branch_variant_data),
            *(get_discriminator(fname, btb_varname, branch_varname, [(btb_prefix + v['name'], v['func_map'][fname]) for v in btb_data.values()], *finfo, classname=classname) for fname, *finfo in btb_variant_data)
        )
       )

# Return a pair containing two generators: The first generates C++ code declaring all functions for the cache modules, and the second generates C++ code defining the functions
def get_cache_module_lines(pref_data, repl_data):
    pref_prefix = 'p'
    pref_varname = 'P_FLAG'

    pref_nonbranch_variant_data = [
        ('prefetcher_initialize',),
        ('prefetcher_cache_operate', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in')), 'uint32_t', 'std::bit_xor'),
        ('prefetcher_cache_fill', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in')), 'uint32_t', 'std::bit_xor'),
        ('prefetcher_cycle_operate',),
        ('prefetcher_final_stats',)
    ]

    pref_branch_variant_data = [
        ('prefetcher_branch_operate', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')))
    ]

    repl_prefix = 'r'
    repl_varname = 'R_FLAG'

    repl_variant_data = [
        ('initialize_replacement',),
        ('find_victim', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type')), 'uint32_t', 'champsim::detail::take_last'),
        ('update_replacement_state', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit'))),
        ('replacement_final_stats',)
    ]

    classname = 'CACHE::module_model<' + pref_varname + ', ' + repl_varname + '>'

    return (
        itertools.chain(
            constants_for_modules(pref_prefix, pref_data.values()), ('',),
            constants_for_modules(repl_prefix, repl_data.values()), ('',),

            # Establish functions common to all prefetchers
            *(get_module_variant_declarations(fname, [v['func_map'][fname] for v in pref_data.values()], *finfo) for fname, *finfo in pref_nonbranch_variant_data),

            # Establish functions that only matter to instruction prefetchers
            ('', '// Assert data prefetchers do not operate on branches'),
            *(mangled_prohibited_definitions(fname, [v['func_map'][fname] for v in pref_data.values() if not v.get('_is_instruction_prefetcher')], *finfo) for fname, *finfo in pref_branch_variant_data),
            *(get_module_variant_declarations(fname, [v['func_map'][fname] for v in pref_data.values() if v.get('_is_instruction_prefetcher')], *finfo) for fname, *finfo in pref_branch_variant_data),

            # Declare name-mangled functions
            *(get_module_variant_declarations(fname, [v['func_map'][fname] for v in repl_data.values()], *finfo) for fname, *finfo in repl_variant_data)
        ),

        itertools.chain(
            *(get_discriminator(fname, pref_varname, repl_varname, [(pref_prefix + v['name'], v['func_map'][fname]) for v in pref_data.values()], *finfo, classname=classname) for fname, *finfo in itertools.chain(pref_nonbranch_variant_data, pref_branch_variant_data)),
            *(get_discriminator(fname, repl_varname, pref_varname, [(repl_prefix + v['name'], v['func_map'][fname]) for v in repl_data.values()], *finfo, classname=classname) for fname, *finfo in repl_variant_data)
        )
       )
