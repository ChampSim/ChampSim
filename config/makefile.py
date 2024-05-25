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
import os

from . import util

def header(values):
    '''
    Generate a makefile section header.

    :param values: A dictionary with the parameters to display
    '''
    yield '######'
    yield from (f'# {k}: {v}' for k,v in values.items())
    yield '######'

def dereference(var):
    ''' Dereference the variable with the given name '''
    return '$(' + var + ')'

def __do_dependency(dependents, targets=None, order_dependents=None, static_pattern=None):
    targets_head, targets_tail = util.cut(targets or [], n=-1)
    pattern_head, pattern_tail = util.cut(static_pattern or [], n=-1)
    orders_head, orders_tail = util.cut(order_dependents or [], n=1)
    targets_tail = (l+':' for l in targets_tail)
    pattern_tail = (l+':' for l in pattern_tail)
    orders_head = ('| '+l for l in orders_head)
    sequence = itertools.chain(targets_head, targets_tail, pattern_head, pattern_tail, dependents, orders_head, orders_tail)
    yield from util.multiline(sequence, length=3, indent=1, line_end=' \\')

def __do_assign_variable(operator, var, val, targets):
    yield from __do_dependency(itertools.chain((f'{var} {operator}',), val), targets)

def dependency(target_iterable, head_dependent, *tail_dependent):
    ''' Mark the target as having the given dependencies '''
    yield from __do_dependency((head_dependent, *tail_dependent), target_iterable)

def assign_variable(var, head_val, *tail_val, targets=None):
    ''' Assign the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable('=', var, (head_val, *tail_val), targets)

def hard_assign_variable(var, head_val, *tail_val, targets=None):
    ''' Assign the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable(':=', var, (head_val, *tail_val), targets)

def append_variable(var, head_val, *tail_val, targets=None):
    ''' Append the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable('+=', var, (head_val, *tail_val), targets)

def relroot(abspath):
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    return os.path.relpath(abspath, start=champsim_root)

def get_makefile_lines(build_id, executable, module_info):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({
        'Build ID': build_id,
        'Executable': executable,
        'Module Names': tuple(module_info.keys())
    })
    yield ''
    exe_dirname, exe_basename = os.path.split(os.path.normpath(executable))
    exe_basename = os.path.join('$(BIN_ROOT)', exe_basename)
    yield from hard_assign_variable('BIN_ROOT', exe_dirname)
    yield from hard_assign_variable('build_id', build_id, targets=[exe_basename])

    mod_paths = [relroot(mod["path"]) for mod in module_info.values()]
    yield from append_variable('nonbase_module_objs', '$(filter-out $(base_module_objs),$(call get_module_list,', *mod_paths, '))')

    legacy_paths = [relroot(mod['path'])+'/' for mod in module_info.values() if mod.get('legacy',False)]
    if legacy_paths:
        yield from append_variable('prereq_for_generated', *legacy_paths, targets=['$(generated_files)'])

    yield from append_variable('executable_name', exe_basename)

    yield ''
