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

def __do_dependency(dependents, targets=None, order_dependents=None):
    targets_head, targets_tail = util.cut(targets or [], n=-1)
    orders_head, orders_tail = util.cut(order_dependents or [], n=1)
    targets_tail = (l+':' for l in targets_tail)
    orders_head = ('| '+l for l in orders_head)
    sequence = itertools.chain(targets_head, targets_tail, dependents, orders_head, orders_tail)
    yield from util.multiline(sequence, length=3, indent=1, line_end=' \\')

def __do_assign_variable(operator, var, val, targets):
    yield from __do_dependency(itertools.chain((f'{var} {operator}',), val), targets)

def dependency(target_iterable, head_dependent, *tail_dependent):
    ''' Mark the target as having the given dependencies '''
    yield from __do_dependency((head_dependent, *tail_dependent), target_iterable)

def assign_variable(var, head_val, *tail_val, targets=None):
    ''' Assign the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable('=', var, (head_val, *tail_val), targets)

def append_variable(var, head_val, *tail_val, targets=None):
    ''' Append the given values space-separated to the given variable, conditionally for the given targets '''
    yield from __do_assign_variable('+=', var, (head_val, *tail_val), targets)

def make_subpart(i, src_dir, base, dest_dir, build_id):
    ''' Map a single directory of sources to destinations. '''
    local_dir_varname = f'{build_id}_dirs_{i}'
    local_obj_varname = f'{build_id}_objs_{i}'

    rel_dest_dir = sanitize(os.path.abspath(os.path.join(dest_dir, os.path.relpath(base, src_dir))))
    rel_src_dir = sanitize(os.path.abspath(src_dir))

    yield from header({'Build ID': build_id, 'Source': rel_src_dir, 'Destination': rel_dest_dir})
    yield ''

    # Define variables
    yield from assign_variable(local_dir_varname, sanitize(dest_dir))

    map_source_to_obj = f'$(patsubst {rel_src_dir}/%.cc, {rel_dest_dir}/%.o, $(wildcard {rel_src_dir}/*.cc))'
    yield from assign_variable(local_obj_varname, map_source_to_obj)

    # Assign dependencies
    wildcard_dep = dependency([os.path.join(rel_dest_dir, '%.o')], os.path.join(rel_src_dir, '%.cc'))
    yield from __do_dependency(wildcard_dep, [dereference(local_obj_varname)], [rel_dest_dir])

    wildcard_dest_dir = os.path.join(rel_dest_dir, '*.d')
    yield f'-include $(wildcard {wildcard_dest_dir})'
    yield ''

    return local_dir_varname, local_obj_varname

def make_part(src_dirs, dest_dir, build_id):
    '''
    Given a list of source directories and a destination directory, generate the makefile linkages to make
    object files across the directories.

    :param src_dirs: a sequence of source directories
    :param dest_dir: the target directory
    :param build_id: a unique identifier for this build
    :returns: a tuple of the list of directory make variable names and the object make variable names
    '''
    source_base = itertools.chain.from_iterable([(s,b) for b,_,_ in os.walk(s)] for s in src_dirs)
    counted_arg_list = ((i, *sb, dest_dir, build_id) for i,sb in enumerate(source_base))

    dir_varnames, obj_varnames = yield from util.yield_from_star(make_subpart, counted_arg_list, n=2)
    return dir_varnames, obj_varnames

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, omit_main):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({
        'Build ID': build_id,
        'Executable': executable,
        'Source Directories': source_dirs,
        'Module Names': list(module_info.keys())
    })
    yield ''

    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    ragged_dir_varnames, ragged_obj_varnames = yield from util.yield_from_star(make_part, (
        (source_dirs, os.path.join(objdir, 'obj'), build_id),
        *(((mod_info['path'],), os.path.join(objdir, name), build_id+'_'+name) for name, mod_info in module_info.items())
    ), n=2)

    # Flatten varnames
    dir_varnames, obj_varnames = list(itertools.chain(*ragged_dir_varnames)), list(itertools.chain(*ragged_obj_varnames))

    options_fname = sanitize(os.path.normpath(os.path.join(objdir, '..', 'absolute.options')))
    global_options_fname = sanitize(os.path.join(champsim_root, 'global.options'))
    global_module_options_fname = sanitize(os.path.join(champsim_root, 'module.options'))
    exec_fname = sanitize(os.path.abspath(executable))

    for var, item in zip(ragged_obj_varnames[1:], module_info.items()):
        name, mod_info = item
        if mod_info.get('legacy'):
            module_options_fname = sanitize(os.path.join(objdir, 'inc', name+'.options'))
            yield from dependency(map(dereference, var), module_options_fname)

    options_names = (global_module_options_fname, options_fname, global_options_fname)
    yield from dependency(map(dereference, itertools.chain(*ragged_obj_varnames[1:])), *options_names)
    yield from dependency(map(dereference, ragged_obj_varnames[0]), *options_names[1:])

    objs = map(dereference, obj_varnames)
    if omit_main:
        objs = itertools.chain(('$(filter-out', '%/main.o,'), map(dereference, obj_varnames), (')',))

    yield from __do_dependency(objs, [exec_fname], [os.path.dirname(exec_fname)])
    yield from append_variable('CPPFLAGS', f'-I{os.path.join(objdir, "inc")}', targets=map(dereference, obj_varnames))

    yield from append_variable('executable_name', exec_fname)
    yield from assign_variable('local_dirs', *map(dereference, dir_varnames), targets=[exec_fname])
    yield from append_variable('dirs', *map(dereference, dir_varnames), os.path.dirname(exec_fname))
    yield from append_variable('objs', *map(dereference, obj_varnames))
    yield ''
