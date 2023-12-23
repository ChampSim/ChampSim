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

from . import util

def header(values):
    '''
    Generate a makefile section header.

    :param values: A dictionary with the parameters to display
    '''
    yield '######'
    yield from (f'# {k}: {v}' for k,v in values.items())
    yield '######'

def __do_dependency(dependents, targets=None, order_dependents=None):
    targets_head, targets_tail = util.cut(targets or [], n=-1)
    orders_head, orders_tail = util.cut(order_dependents or [], n=1)
    targets_tail = (l+':' for l in targets_tail)
    orders_head = ('| '+l for l in orders_head)
    sequence = itertools.chain(targets_head, targets_tail, dependents, orders_head, orders_tail)
    yield from util.multiline(sequence, length=3, indent=1, line_end=' \\')

def __do_assign_variable(operator, var, val, targets):
    yield from __do_dependency(itertools.chain((f'{var} {operator}',), val), targets)

def append_variable(var, head_val, *tail_val, targets=None):
    ''' Append the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable('+=', var, (head_val, *tail_val), targets)

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, omit_main):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({
        'Build ID': build_id,
        'Executable': executable,
        'Source Directories': source_dirs,
        'Module Names': tuple(module_info.keys())
    })
    yield ''
    yield f'$(DEP_ROOT)/{build_id}.mkpart: source_roots = {" ".join(source_dirs)} {" ".join("MODULE "+m["path"] for m in module_info.values())}'
    yield f'$(DEP_ROOT)/{build_id}.mkpart: exe = {executable}'
    yield f'$(DEP_ROOT)/{build_id}.mkpart: $(source_roots)'
    yield f'include $(DEP_ROOT)/{build_id}.mkpart'

    yield from append_variable('executable_name', executable)
    yield ''
