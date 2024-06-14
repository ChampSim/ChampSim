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
        paths = itertools.chain(
            (os.path.join(dirname, module) for dirname in self.paths), # Prepend search paths
            (module,) # Interpret as file path
        )

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

listener_variant_data = [
    ('process_event', (('event', 'eventType'), ('void*', 'data')), 'void')
]
def get_listener_data(module_data):
    func_map = { v[0]: f'r_{module_data["name"]}_{v[0]}' for v in listener_variant_data }
    return util.chain(module_data, { 'func_map': func_map })
