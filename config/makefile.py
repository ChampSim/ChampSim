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

def sanitize(name):
    ''' Remove colon characters from makefile names. '''
    return name.replace(':', r'\:')

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

def make_subpart(i, base, sub_src, sub_dest, build_id):
    ''' Map a single directory of sources to destinations. '''
    local_obj_varname = f'{build_id}_objs_{i}'
    rel_dest_dir = sanitize(os.path.join('$(OBJ_ROOT)', sub_dest) if sub_dest else '$(OBJ_ROOT)')
    sub_dest = os.path.join(sub_dest, sub_src) if sub_src else os.path.normpath(sub_dest)
    rel_src_dir = sanitize(os.path.normpath(os.path.join(base, sub_src)))

    yield from header({'Build ID': build_id, 'Source': rel_src_dir, 'Destination': sub_dest})
    yield ''

    # Define variables
    yield from append_variable('dirs', *util.path_ancestors(rel_dest_dir))

    map_source_to_obj = f'$(call migrate,{rel_src_dir},{rel_dest_dir},.cc,.o,{build_id})'
    yield from hard_assign_variable(local_obj_varname, map_source_to_obj)

    # Assign dependencies
    yield from __do_dependency(
        targets=[f'$(filter %main.o, {dereference(local_obj_varname)})'],
        static_pattern=[f'{os.path.join(rel_dest_dir, f"{build_id}_%.o")}'],
        order_dependents=[rel_dest_dir],
        dependents=[os.path.join(rel_src_dir, '%.cc'), f'$(wildcard {os.path.join(rel_src_dir, "*.options")})']
    )
    yield from __do_dependency(
        targets=[f'$(filter-out %main.o, {dereference(local_obj_varname)})'],
        static_pattern=[f'{os.path.join(rel_dest_dir, f"%.o")}'],
        order_dependents=[rel_dest_dir],
        dependents=[os.path.join(rel_src_dir, '%.cc'), f'$(wildcard {os.path.join(rel_src_dir, "*.options")})']
    )
    yield ''

    return local_obj_varname

def make_part(src_dirs, dest_dir, build_id):
    '''
    Given a list of source directories and a destination directory, generate the makefile linkages to make
    object files across the directories.

    :param src_dirs: a sequence of source directories
    :param dest_dir: the target directory
    :param build_id: a unique identifier for this build
    :returns: a tuple of the list of directory make variable names and the object make variable names
    '''
    source_base = itertools.chain.from_iterable([(os.path.abspath(s),os.path.relpath(b,s)) for b,_,_ in os.walk(s)] for s in src_dirs)
    counted_arg_list = ((i, source, base, dest_dir, build_id) for i,(source, base) in enumerate(source_base))

    obj_varnames = yield from util.yield_from_star(make_subpart, counted_arg_list, n=1)
    return list(itertools.chain(*obj_varnames))

def flatten_ragged(to_flatten):
    return list(itertools.chain.from_iterable(to_flatten))

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, pmem):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({
        'Build ID': build_id,
        'Executable': executable,
        'Source Directories': source_dirs,
        'Module Names': tuple(module_info.keys())
    })
    yield ''

    exec_dir, exec_fname = os.path.split(sanitize(os.path.abspath(executable)))
    exec_fname = os.path.join('$(BIN_ROOT)', exec_fname)

    yield from hard_assign_variable('BIN_ROOT', exec_dir)
    yield from hard_assign_variable('OBJ_ROOT', os.path.normpath(objdir))
    yield ''
    ragged_obj_varnames = yield from util.yield_from_star(make_part, (
        (source_dirs, '', build_id),
        *(((mod_info['path'],), name, build_id+'_'+name) for name, mod_info in module_info.items())
    ), n=1)

    # Flatten once
    ragged_obj_varnames = flatten_ragged(ragged_obj_varnames)

    # Flatten varnames
    obj_varnames = flatten_ragged(ragged_obj_varnames)

    options_fname = sanitize(os.path.join('$(ROOT_DIR)', 'absolute.options'))
    global_options_fname = sanitize(os.path.join('$(ROOT_DIR)', 'global.options'))
    global_module_options_fname = sanitize(os.path.join('$(ROOT_DIR)', 'module.options'))

    options_names = (global_module_options_fname, options_fname, global_options_fname)
    yield from dependency(map(dereference, itertools.chain(*ragged_obj_varnames[1:])), *options_names)
    yield from dependency(map(dereference, ragged_obj_varnames[0]), *options_names[1:])

    objs = map(dereference, obj_varnames)

    yield from dependency([exec_fname], *objs)
    yield from append_variable('CPPFLAGS', f'-I{objdir}', targets=map(dereference, obj_varnames))
    yield from append_variable('CPPFLAGS', f'-DCHAMPSIM_BUILD=0x{build_id}', targets=[f'$(filter %main.o, {" ".join(map(dereference, obj_varnames))})'])


    #ramulator support (add DRAMULATOR to flags and set model flag)
    if pmem['model'] == 'ramulator':
        yield from assign_variable('RAMULATOR_MODEL', '1')

    yield from append_variable('CPPFLAGS', f'-I{os.path.join(objdir, "inc")}', targets=map(dereference, obj_varnames))
    yield from append_variable('executable_name', exec_fname)
    yield from append_variable('dirs', os.path.dirname(exec_fname))
    yield from append_variable('objs', *map(dereference, obj_varnames))
    yield ''
