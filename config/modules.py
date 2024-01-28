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
import functools

from . import util
from . import cxx

def get_module_name(path, start=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))):
    ''' Create a mangled module name from the path to its sources '''
    fname_translation_table = str.maketrans('./-','_DH')
    return os.path.relpath(path, start=start).translate(fname_translation_table)

class ModuleSearchContext:
    def __init__(self, paths, verbose=False):
        self.paths = [p for p in paths if os.path.exists(p) and os.path.isdir(p)]
        self.verbose = verbose

    def data_from_path(self, path):
        name = get_module_name(path)
        is_legacy = ('__legacy__' in [*itertools.chain(*(f for _,_,f in os.walk(path)))])
        retval = {
            'name': name,
            'path': path,
            'legacy': is_legacy,
            'class': 'champsim::modules::generated::'+name if is_legacy else os.path.basename(path)
        }

        if self.verbose:
            print('M:', retval)
        return retval

    # Try the context's module directories, then try to interpret as a path
    def find(self, module):
        # Return a normalized directory: variables and user shorthands are expanded
        paths = list(itertools.chain(
            (os.path.join(dirname, module) for dirname in self.paths), # Prepend search paths
            (module,) # Interpret as file path
        ))

        paths = map(os.path.expandvars, paths)
        paths = map(os.path.expanduser, paths)
        paths = filter(os.path.exists, paths)
        path = os.path.relpath(next(paths, None))

        return self.data_from_path(path)

    def find_all(self):
        base_dirs = [next(os.walk(p)) for p in self.paths]
        files = itertools.starmap(os.path.join, itertools.chain(*(zip(itertools.repeat(b), d) for b,d,_ in base_dirs)))
        return [self.data_from_path(f) for f in files]

branch_variant_data = [
    ('initialize_branch_predictor', tuple(), 'void'),
    ('last_branch_result', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type')), 'void'),
    ('predict_branch', (('uint64_t','ip'),), 'uint8_t')
]
def get_branch_data(module_data):
    func_map = { v[0]: f'b_{module_data["name"]}_{v[0]}' for v in branch_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

btb_variant_data = [
    ('initialize_btb', tuple(), 'void'),
    ('update_btb', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type')), 'void'),
    ('btb_prediction', (('uint64_t','ip'),), 'std::pair<uint64_t, uint8_t>')
]
def get_btb_data(module_data):
    func_map = { v[0]: f't_{module_data["name"]}_{v[0]}' for v in btb_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

pref_nonbranch_variant_data = [
    ('prefetcher_initialize', tuple(), 'void'),
    ('prefetcher_cache_operate', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('bool', 'useful_prefetch'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cache_fill', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cycle_operate', tuple(), 'void'),
    ('prefetcher_final_stats', tuple(), 'void')
]

pref_branch_variant_data = [
    ('prefetcher_branch_operate', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')), 'void')
]
def get_pref_data(module_data):
    prefix = 'ipref' if module_data.get('_is_instruction_prefetcher', False) else 'pref'
    func_map = { v[0]: f'{prefix}_{module_data["name"]}_{v[0]}'
        for v in itertools.chain(pref_branch_variant_data, pref_nonbranch_variant_data) }

    return util.chain(module_data,
        { 'func_map': func_map },
        { 'deprecated_func_map' : {
                'l1i_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l1d_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l2c_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'llc_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l1i_prefetcher_cache_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l1d_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l2c_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'llc_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l1i_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l1d_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l2c_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'llc_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l1i_prefetcher_cycle_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cycle_operate')),
                'l1i_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l1d_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l2c_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'llc_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l1i_prefetcher_branch_operate': '_'.join((prefix, module_data['name'], 'prefetcher_branch_operate'))
            }
        }
    )

repl_variant_data = [
    ('initialize_replacement', tuple(), 'void'),
    ('find_victim', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const CACHE::BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type')), 'uint32_t'),
    ('update_replacement_state', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit')), 'void'),
    ('replacement_final_stats', tuple(), 'void')
]
def get_repl_data(module_data):
    func_map = { v[0]: f'r_{module_data["name"]}_{v[0]}' for v in repl_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

sm_variant_data = [
    ('initialize_state_model', tuple(), 'void'),
    ('state_model_final_stats', tuple(), 'void')
]
def get_sm_data(module_data): 
    func_map = { v[0]: f'r_{module_data["name"]}_{v[0]}' for v in sm_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

###
# Legacy module support below
###

def get_legacy_module_opts_lines(module_data):
    '''
    Generate an iterable of the compiler options for a particular module
    '''
    full_funcmap = util.chain(module_data['func_map'], module_data.get('deprecated_func_map', {}))
    yield from  (f'-D{k}={v}' for k,v in full_funcmap.items())

def mangled_declaration(fname, args, rtype, module_data):
    ''' Generate C++ code giving the mangled module specialization functions. '''
    argstring = ', '.join(a[0] for a in args)
    return f'{rtype} {module_data["func_map"][fname]}({argstring});'

def variant_function_body(fname, args, module_data):
    argnamestring = ', '.join(a[1] for a in args)
    body = [f'return intern_->{module_data["func_map"][fname]}({argnamestring});']
    yield from cxx.function(fname, body, args=args)
    yield ''

def get_discriminator(variant_data, module_data, classname):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    discriminator_classname = module_data['class'].split('::')[-1]
    body = itertools.chain(
        (f'using {classname}::{classname};',),
        *(variant_function_body(n,a,module_data) for n,a,_ in variant_data)
    )
    yield from cxx.struct(discriminator_classname, body, superclass=classname)
    yield ''

def get_legacy_module_lines(branch_data, btb_data, pref_data, repl_data):
    '''
    Create three generators:
      - The first generates C++ code declaring all functions for the O3_CPU modules,
      - The second generates C++ code declaring all functions for the CACHE modules,
      - The third generates C++ code defining the functions.
    '''
    branch_discriminator = functools.partial(get_discriminator, branch_variant_data, classname='branch_predictor')
    btb_discriminator = functools.partial(get_discriminator, btb_variant_data, classname='btb')
    repl_discriminator = functools.partial(get_discriminator, repl_variant_data, classname='replacement')
    #sm_discriminator = functools.partial(get_discriminator, sm_variant_data, classname='state_model')

    def pref_discriminator(v):
        local_branch_variant_data = pref_branch_variant_data if v.get('_is_instruction_prefetcher') else []
        return get_discriminator([*pref_nonbranch_variant_data, *local_branch_variant_data], v, classname='prefetcher')

    return (
        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(branch_variant_data, branch_data),
            itertools.product(btb_variant_data, btb_data)
        )),

        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(pref_nonbranch_variant_data + pref_branch_variant_data, pref_data),
            itertools.product(repl_variant_data, repl_data)
        )),

        itertools.chain(
            *map(branch_discriminator, branch_data),
            *map(btb_discriminator, btb_data),
            *map(pref_discriminator, pref_data),
            *map(repl_discriminator, repl_data)
            #*map(sm_discriminator, repl_data)
        )
       )
