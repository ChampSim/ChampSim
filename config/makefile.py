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

def multiline(long_line, length=1):
    ''' Split a long string into lines with n words '''
    grouped = [iter(long_line)] * length
    grouped = itertools.zip_longest(*grouped, fillvalue='')
    grouped = (' '.join(filter(None, group)) for group in grouped)
    yield from util.append_except_last(grouped, ' \\')

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
    joined_vals = multiline((head_dependent, *tail_dependent), length=3)
    yield from util.do_for_first(lambda first: f'{target}: {first}', joined_vals)

def order_dependency(target, head_dependent, *tail_dependent):
    ''' Mark the target as having the given order-only dependencies '''
    joined_vals = multiline((head_dependent, *tail_dependent), length=3)
    yield from util.do_for_first(lambda first: f'{target}: | {first}', joined_vals)

def assign_variable(var, head_val, *tail_val, targets=None):
    ''' Assign the given values space-separated to the given variable, conditionally for the given targets '''
    joined_vals = multiline(itertools.chain((head_val,), tail_val), length=3)
    variable_append = util.do_for_first(lambda first: f'{var} = {first}', joined_vals)
    if targets is None:
        yield from variable_append
    else:
        yield from util.do_for_first(lambda first: f'{targets}: {first}', variable_append)

def append_variable(var, head_val, *tail_val, targets=None):
    ''' Append the given values space-separated to the given variable, conditionally for the given targets '''
    joined_vals = multiline(itertools.chain((head_val,), tail_val), length=3)
    variable_append = util.do_for_first(lambda first: f'{var} += {first}', joined_vals)
    if targets is None:
        yield from variable_append
    else:
        yield from util.do_for_first(lambda first: f'{targets}: {first}', variable_append)

def make_subpart(i, src_dir, base, dest_dir, build_id):
    local_dir_varname = f'{build_id}_dirs_{i}'
    local_obj_varname = f'{build_id}_objs_{i}'

    rel_dest_dir = os.path.abspath(os.path.join(dest_dir, os.path.relpath(base, src_dir)))
    rel_src_dir = os.path.abspath(src_dir)

    yield from header({'Build ID': build_id, 'Source': rel_src_dir, 'Destination': rel_dest_dir})
    yield ''

    # Define variables
    yield from assign_variable(local_dir_varname, dest_dir)

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

def make_part(src_dirs, dest_dir, build_id):
    '''
    Given a list of source directories and a destination directory, generate the makefile linkages to make
    object files across the directories.

    :param src_dirs: a sequence of source directories
    :param dest_dir: the target directory
    :param build_id: a unique identifier for this build
    :returns: a tuple of the list of directory make variable names and the object make variable names
    '''
    dir_varnames = []
    obj_varnames = []

    for i, base_source in enumerate(itertools.chain(*([(s,b) for b,_,_ in os.walk(s)] for s in src_dirs))):
        local_dir_varname, local_obj_varname = yield from make_subpart(i, *base_source, dest_dir, build_id)
        dir_varnames.append(local_dir_varname)
        obj_varnames.append(local_obj_varname)

    return dir_varnames, obj_varnames

def make_all_parts(*part_parameters):
    ''' Generate each part for a tuple of parameters to make_part(), returning a list of lists of the variable names. '''
    dir_varnames, obj_varnames = [], []
    for params in part_parameters:
        part_dir_varnames, part_obj_varnames = yield from make_part(*params)
        dir_varnames.append(part_dir_varnames)
        obj_varnames.append(part_obj_varnames)

    return dir_varnames, obj_varnames

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info):
    ''' Generate all of the lines to be written in a particular configuration's makefile '''
    yield from header({'Build ID': build_id, 'Executable': executable})
    yield ''

    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    dir_varnames, obj_varnames = yield from make_all_parts(
        (source_dirs, os.path.join(objdir, 'obj'), build_id),
        *(((mod_info['fname'],), os.path.join(objdir, name), build_id+'_'+name) for name, mod_info in module_info.items())
    )

    for var, name in zip(obj_varnames[1:], module_info.keys()):
        yield from dependency(' '.join(map(dereference, var)), os.path.join(objdir, 'inc', name, 'config.options'))

    # Flatten varnames
    dir_varnames, obj_varnames = list(itertools.chain(*dir_varnames)), list(itertools.chain(*obj_varnames))

    options_fname = os.path.join(objdir, 'inc', 'config.options')
    global_options_fname = os.path.join(champsim_root, 'global.options')

    yield from dependency(' '.join(map(dereference, obj_varnames)), global_options_fname, options_fname)
    yield from dependency(os.path.abspath(executable), *map(dereference, obj_varnames))
    yield from order_dependency(os.path.abspath(executable), os.path.abspath(os.path.dirname(executable)))
    yield from append_variable('executable_name', os.path.abspath(executable))
    yield from append_variable('dirs', *map(dereference, dir_varnames), os.path.abspath(os.path.dirname(executable)))
    yield from append_variable('objs', *map(dereference, obj_varnames))
    yield ''
