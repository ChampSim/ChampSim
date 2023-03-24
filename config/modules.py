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

# Utility function to get a mangled module name from the path to its sources
def get_module_name(path, start=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))):
    fname_translation_table = str.maketrans('./-','_DH')
    return os.path.relpath(path, start=start).translate(fname_translation_table)

class ModuleSearchContext:
    def __init__(self, paths):
        self.paths = [p for p in paths if os.path.exists(p) and os.path.isdir(p)]

    def data_from_path(self, path):
        return {'name': get_module_name(path), 'fname': path, 'legacy': ('__legacy__' in [*itertools.chain(*(f for _,_,f in os.walk(path)))])}

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

branch_variant_data = [
    ('initialize_branch_predictor', tuple(), 'void'),
    ('last_branch_result', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type')), 'void'),
    ('predict_branch', (('uint64_t','ip'),), 'uint8_t')
]
def get_branch_data(module_name):
    return data_getter('bpred', module_name, (v[0] for v in branch_variant_data))

btb_variant_data = [
    ('initialize_btb', tuple(), 'void'),
    ('update_btb', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type')), 'void'),
    ('btb_prediction', (('uint64_t','ip'),), 'std::pair<uint64_t, uint8_t>')
]
def get_btb_data(module_name):
    return data_getter('btb', module_name, (v[0] for v in btb_variant_data))

pref_nonbranch_variant_data = [
    ('prefetcher_initialize', tuple(), 'void'),
    ('prefetcher_cache_operate', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cache_fill', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cycle_operate', tuple(), 'void'),
    ('prefetcher_final_stats', tuple(), 'void')
]

pref_branch_variant_data = [
    ('prefetcher_branch_operate', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')), 'void')
]
def get_pref_data(module_name, is_instruction_cache=False):
    return data_getter('ipref' if is_instruction_cache else 'pref', module_name, (v[0] for v in itertools.chain(pref_branch_variant_data, pref_nonbranch_variant_data)))

repl_variant_data = [
    ('initialize_replacement', tuple(), 'void'),
    ('find_victim', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const CACHE::BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type')), 'uint32_t'),
    ('update_replacement_state', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit')), 'void'),
    ('replacement_final_stats', tuple(), 'void')
]
def get_repl_data(module_name):
    return data_getter('repl', module_name, (v[0] for v in repl_variant_data))

# Generate C++ code giving the mangled module specialization functions
def mangled_declarations(variant_data, module_data):
    for fname, args, rtype in variant_data:
        yield '{} {}({});'.format(rtype, module_data['func_map'][fname], ', '.join(a[0] for a in args))

# For a given module function, generate C++ code defining the discriminator function
def get_discriminator(variant_data, module_data, classname):
    yield 'struct {} : {}'.format(module_data['name'], classname)
    yield '{'
    yield '  using {0}::{0};'.format(classname)

    for fname, args, _ in variant_data:
        argstring = ', '.join((a[0]+' '+a[1]) for a in args)
        yield '  auto {}({})'.format(fname, argstring)
        yield '  {'
        yield '    return intern_->{}({});'.format(module_data['func_map'][fname], ', '.join(a[1] for a in args))
        yield '  }'
        yield ''

    yield '};'
    yield ''

# Return a pair containing two generators: The first generates C++ code declaring all functions for the O3_CPU modules, and the second generates C++ code defining the functions
def get_ooo_cpu_module_lines(branch_data, btb_data):
    return (
        itertools.chain(
            # Declare name-mangled functions
            *(mangled_declarations(branch_variant_data, v) for v in branch_data.values()),
            *(mangled_declarations(btb_variant_data, v) for v in btb_data.values())
        ),

        itertools.chain(
            ('namespace champsim::modules::generated','{'),
            *(get_discriminator(branch_variant_data, v, 'branch_predictor') for v in branch_data.values()),
            *(get_discriminator(btb_variant_data, v, 'btb') for v in btb_data.values()),
            ('}',)
        )
       )

# Return a pair containing two generators: The first generates C++ code declaring all functions for the cache modules, and the second generates C++ code defining the functions
def get_cache_module_lines(pref_data, repl_data):
    return (
        itertools.chain(
            # Declare name-mangled functions
            *(mangled_declarations(pref_nonbranch_variant_data + pref_branch_variant_data, v) for v in pref_data.values()),
            *(mangled_declarations(repl_variant_data, v) for v in repl_data.values())
        ),

        itertools.chain(
            ('namespace champsim::modules::generated','{'),
            *(get_discriminator(pref_nonbranch_variant_data + pref_branch_variant_data, v, 'prefetcher') for v in pref_data.values() if v.get('_is_instruction_prefetcher')),
            *(get_discriminator(pref_nonbranch_variant_data, v, 'prefetcher') for v in pref_data.values() if not v.get('_is_instruction_prefetcher')),
            *(get_discriminator(repl_variant_data, v, 'replacement') for v in repl_data.values()),
            ('}',)
        )
       )
