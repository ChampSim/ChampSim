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

def sanitize(s):
    return s.replace(':', '\:')

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

def dependency(target, head_dependent, *tail_dependent):
    ''' Mark the target as having the given dependencies '''
    joined_vals = util.multiline((head_dependent, *tail_dependent), length=3, indent=1)
    yield from util.do_for_first(lambda first: f'{target}: {first}', joined_vals)

def order_dependency(target, head_dependent, *tail_dependent):
    ''' Mark the target as having the given order-only dependencies '''
    joined_vals = util.multiline((head_dependent, *tail_dependent), length=3, indent=1)
    yield from util.do_for_first(lambda first: f'{target}: | {first}', joined_vals)

def assign_variable(var, head_val, *tail_val, targets=None):
    ''' Assign the given values space-separated to the given variable, conditionally for the given targets '''
    joined_vals = util.multiline(itertools.chain((head_val,), tail_val), length=3, indent=1)
    variable_append = util.do_for_first(lambda first: f'{var} = {first}', joined_vals)
    if targets is None:
        yield from variable_append
    else:
        yield from util.do_for_first(lambda first: f'{targets}: {first}', variable_append)

def append_variable(var, head_val, *tail_val, targets=None):
    ''' Append the given values space-separated to the given variable, conditionally for the given targets '''
    joined_vals = util.multiline(itertools.chain((head_val,), tail_val), length=3, indent=1)
    variable_append = util.do_for_first(lambda first: f'{var} += {first}', joined_vals)
    if targets is None:
        yield from variable_append
    else:
        yield from util.do_for_first(lambda first: f'{targets}: {first}', variable_append)

def make_subpart(i, src_dir, base, dest_dir, build_id):
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
    wildcard_dep = next(dependency(os.path.join(rel_dest_dir, '%.o'), os.path.join(rel_src_dir, '%.cc')), None)
    yield from dependency(dereference(local_obj_varname), wildcard_dep)
    yield from order_dependency(dereference(local_obj_varname), rel_dest_dir)

    wildcard_dest_dir = os.path.join(rel_dest_dir, '*.d')
    yield f'-include $(wildcard {wildcard_dest_dir})'
    yield ''

    return local_dir_varname, local_obj_varname

def yield_from_star(func, args, n=2):
    ''' Generate each part for a tuple of parameters to the given func, returning a list of lists of the variable names. '''
    retvals = [list() for _ in range(n)]
    for a in args:
        retval = yield from func(*a)
        for seq,r in zip(retvals, retval):
            seq.append(r)
    return retvals

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

    dir_varnames, obj_varnames = yield from yield_from_star(make_subpart, counted_arg_list, n=2)
    return dir_varnames, obj_varnames

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, omit_main):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({'Build ID': build_id, 'Executable': executable, 'Source Directories': source_dirs, 'Module Names': list(module_info.keys())})
    yield ''

    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    ragged_dir_varnames, ragged_obj_varnames = yield from yield_from_star(make_part, (
        (source_dirs, objdir, build_id),
        *(((mod_info['path'],), os.path.join(objdir, name), build_id+'_'+name) for name, mod_info in module_info.items())
    ), n=2)

    # Flatten varnames
    dir_varnames, obj_varnames = list(itertools.chain(*ragged_dir_varnames)), list(itertools.chain(*ragged_obj_varnames))

    options_fname = sanitize(os.path.abspath(os.path.join(objdir, 'inc', 'config.options')))
    global_options_fname = sanitize(os.path.abspath(os.path.join(champsim_root, 'global.options')))
    exec_fname = sanitize(os.path.abspath(executable))

    for var, name in zip(ragged_obj_varnames[1:], module_info.keys()):
        module_options_fname = sanitize(os.path.join(objdir, 'inc', name, 'config.options'))
        yield from dependency(' '.join(map(dereference, var)), module_options_fname)
    yield from dependency(' '.join(map(dereference, obj_varnames)), options_fname, global_options_fname)

    objs = map(dereference, obj_varnames)
    if omit_main:
        objs = itertools.chain(('$(filter-out', '%/main.o,'), map(dereference, obj_varnames), (')',))

    yield from dependency(exec_fname, *objs)
    yield from order_dependency(exec_fname, os.path.dirname(exec_fname))

    yield from append_variable('executable_name', exec_fname)
    yield from append_variable('dirs', *map(dereference, dir_varnames), os.path.dirname(exec_fname))
    yield from append_variable('objs', *map(dereference, obj_varnames))
    yield ''
