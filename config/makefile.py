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

def dereference(var):
    return '$(' + var + ')'

def dependency(target, *dependent, order=None):
    if order is None:
        return '{}: {}'.format(target, ' '.join(dependent))
    else:
        return '{}: {} | {}'.format(target, ' '.join(dependent), order)

def assign_variable(var, val, target=None):
    retval = '{} = {}'.format(var, val)
    if target is not None:
        retval = dependency(target, retval)
    return retval

def append_variable(var, *val, targets=[]):
    retval = '{} += {}'.format(var, ' '.join(val))
    if targets:
        retval = dependency(' '.join(targets), retval)
    return retval

def make_part(dest_dir, build_id, src_dirs):
    dir_varnames = []
    obj_varnames = []

    for i, base_source in enumerate(itertools.chain(*([(s,b) for b,_,_ in os.walk(s)] for s in src_dirs))):
        local_dir_varname = '{}_dirs_{}'.format(build_id, i)
        local_obj_varname = '{}_objs_{}'.format(build_id, i)

        src_dir, base = base_source
        reldir = os.path.relpath(base, src_dir)

        rel_dest_dir = os.path.abspath(os.path.join(dest_dir, reldir))
        rel_src_dir = os.path.abspath(src_dir)

        yield '###'
        yield '# Build ID: ' + build_id
        yield '# Source: ' + rel_src_dir
        yield '# Destination: ' + rel_dest_dir
        yield '###'
        yield ''

        # Definee variables
        yield assign_variable(local_dir_varname, dest_dir)
        yield assign_variable(local_obj_varname, '$(patsubst {src_dir}/%.cc, {dest_dir}/%.o, $(wildcard {src_dir}/*.cc))'.format(dest_dir=rel_dest_dir, src_dir=rel_src_dir))

        # Assign dependencies
        yield dependency(dereference(local_obj_varname), dependency(os.path.join(rel_dest_dir, '%.o'), os.path.join(rel_src_dir, '%.cc')), order=rel_dest_dir)
        yield '-include $(wildcard {})'.format(os.path.join(rel_dest_dir, '*.d'))
        yield ''

        dir_varnames.append(local_dir_varname)
        obj_varnames.append(local_obj_varname)

    yield dependency(' '.join(map(dereference, obj_varnames)), os.path.join(dest_dir, 'config.options'))
    yield ''
    return dir_varnames, obj_varnames

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info):
    executable_path = os.path.abspath(executable)

    yield '######'
    yield '# Build ID: ' + build_id
    yield '# Executable: ' + executable_path
    yield '######'
    yield ''

    part_iter = (
        (os.path.join(objdir, 'obj'), build_id, source_dirs),
        *((os.path.join(objdir, k), build_id+'_'+k, (v['path'],)) for k,v in module_info.items())
    )
    dir_varnames = []
    obj_varnames = []
    for part in part_iter:
        module_dir_varnames, module_obj_varnames = yield from make_part(*part)
        dir_varnames.extend(module_dir_varnames)
        obj_varnames.extend(module_obj_varnames)

    yield append_variable('executable_name', executable_path)
    yield append_variable('clean_dirs', *map(dereference, dir_varnames))
    yield append_variable('objs', *map(dereference, obj_varnames))
    yield append_variable('build_dirs', os.path.split(executable_path)[0])
    yield append_variable('config_dirs', objdir)

    yield dependency(executable_path, *map(dereference, obj_varnames), order=os.path.split(executable_path)[0])
    yield dependency(' '.join(map(dereference, obj_varnames)), os.path.join(objdir, 'config.options'))
    yield ''

