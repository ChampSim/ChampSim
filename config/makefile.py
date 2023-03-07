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

import itertools, operator
import os

from . import util

def extend_each(x,y):
    merges = {k: (*x[k],*y[k]) for k in x if k in y}
    return {**x, **y, **merges}

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

def each_in_dict_list(d):
    yield from itertools.chain(*(zip(itertools.repeat(kv[0]), kv[1]) for kv in d.items()))

def make_part(src_dirs, dest_dir, build_id):
    dir_varnames = []
    obj_varnames = []

    for i, base_source in enumerate(itertools.chain(*([(s,b) for b,_,_ in os.walk(s)] for s in src_dirs))):
        local_dir_varname = '{}_dirs_{}'.format(build_id, i)
        local_obj_varname = '{}_objs_{}'.format(build_id, i)

        src_dir, base = base_source
        reldir = os.path.relpath(base, src_dir)

        rel_dest_dir = os.path.abspath(os.path.join(dest_dir, reldir))
        rel_src_dir = os.path.abspath(src_dir)

        local_opts = {'CPPFLAGS': ('-I'+rel_src_dir,) }

        yield '###'
        yield '# Build ID: ' + build_id
        yield '# Source: ' + rel_src_dir
        yield '# Destination: ' + rel_dest_dir
        yield '###'
        yield ''

        # Definee variables
        yield assign_variable(local_dir_varname, dest_dir)
        yield assign_variable(local_obj_varname, '$(patsubst {src_dir}/%.cc, {dest_dir}/%.o, $(wildcard {src_dir}/*.cc))'.format(dest_dir=rel_dest_dir, src_dir=rel_src_dir))

        # Set flags
        yield from (append_variable(*kv, targets=[dereference(local_obj_varname)]) for kv in each_in_dict_list(local_opts))

        # Assign dependencies
        yield dependency(dereference(local_obj_varname), dependency(os.path.join(rel_dest_dir, '%.o'), os.path.join(rel_src_dir, '%.cc')), order=rel_dest_dir)
        yield '-include $(wildcard {})'.format(os.path.join(rel_dest_dir, '*.d'))
        yield ''

        dir_varnames.append(local_dir_varname)
        obj_varnames.append(local_obj_varname)

    return dir_varnames, obj_varnames

def executable_opts(obj_root, build_id, executable, source_dirs):
    dest_dir = os.path.join(obj_root, build_id)

    # Add compiler flags
    local_opts = {'CPPFLAGS': ('-I'+os.path.join(dest_dir, 'inc'),)}

    yield '######'
    yield '# Build ID: ' + build_id
    yield '# Executable: ' + executable
    yield '######'
    yield ''

    dir_varnames, obj_varnames = yield from make_part(source_dirs, os.path.join(dest_dir, 'obj'), build_id)
    yield dependency(executable, *map(dereference, obj_varnames), order=os.path.split(executable)[0])

    yield from (append_variable(*kv, targets=[dereference(x) for x in obj_varnames]) for kv in each_in_dict_list(local_opts))
    yield append_variable('build_dirs', *map(dereference, dir_varnames), os.path.split(executable)[0])
    yield append_variable('build_objs', *map(dereference, obj_varnames))
    yield append_variable('executable_name', executable)
    yield ''

    return dir_varnames, obj_varnames

def module_opts(obj_dir, build_id, module_name, source_dirs, opts):
    build_dir = os.path.join(obj_dir, build_id)
    dest_dir = os.path.join(build_dir, module_name)

    local_opts = {'CPPFLAGS': ('-I'+os.path.join(build_dir, 'inc'), '-include {}.inc'.format(module_name))}

    dir_varnames, obj_varnames = yield from make_part(source_dirs, dest_dir, build_id+'_'+module_name)
    yield from (append_variable(*kv, targets=[dereference(x) for x in obj_varnames]) for kv in each_in_dict_list(opts))
    yield from (append_variable(*kv, targets=[dereference(x) for x in obj_varnames]) for kv in each_in_dict_list(local_opts))
    yield append_variable('module_dirs', *map(dereference, dir_varnames))
    yield append_variable('module_objs', *map(dereference, obj_varnames))
    yield ''

    return dir_varnames, obj_varnames

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, config_file):
    executable_path = os.path.abspath(executable)

    dir_varnames, obj_varnames = yield from executable_opts(os.path.abspath(objdir), build_id, executable_path, source_dirs)
    for k,v in module_info.items():
        module_dir_varnames, module_obj_varnames = yield from module_opts(os.path.abspath(objdir), build_id, k, (v['fname'],), v['opts'])
        yield dependency(executable_path, *map(dereference, module_obj_varnames))
        dir_varnames.extend(module_dir_varnames)
        obj_varnames.extend(module_obj_varnames)

    global_overrides = util.subdict(config_file, ('CXX',))
    yield from (assign_variable(*kv, targets=[dereference(x) for x in dir_varnames]) for kv in global_overrides.items())

    global_opts = util.subdict(config_file, ('CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS'))
    yield from (append_variable(*kv, targets=[dereference(x) for x in obj_varnames]) for kv in each_in_dict_list(global_opts))
    yield ''

