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

from .util import extend_each

def walk_to(source_dir, dest_dir, extensions=('.cc',)):
    for base,dirs,files in os.walk(source_dir):
        reldir = os.path.join(dest_dir, os.path.relpath(base, source_dir))
        yield reldir, [os.path.normpath(os.path.join(reldir, f)+'.o') for f,ext in map(os.path.splitext, files) if ext in extensions]

def linejoin(elems):
    return '\\\n  '.join(elems)

per_source_fmtstr = (
'''{local_dir_varname} = {dirs}
{local_obj_varname} = {objs}
$({local_obj_varname}): {dest_dir}/%.o: {src_dir}/%.cc | $({local_dir_varname})
-include $(wildcard {dest_dir}/*.d))
'''
)
def per_source(src_dirs, dest_dir, build_id):
    for i, src_dir in enumerate(src_dirs):
        local_dir_varname = '{}_dirs_{}'.format(build_id, i)
        local_obj_varname = '{}_objs_{}'.format(build_id, i)

        obj_dirnames, obj_filenames = next(walk_to(src_dir, dest_dir))

        dirs = os.path.normpath(obj_dirnames)
        objs = linejoin(sorted(obj_filenames))

        yield local_dir_varname, local_obj_varname, per_source_fmtstr.format(
                local_dir_varname=local_dir_varname,
                local_obj_varname=local_obj_varname,
                dirs=dirs,
                objs=objs,
                dest_dir=dest_dir,
                src_dir=src_dir
                )

def make_part(source_dirs, obj_dir, build_id, executable, part_opts, part_overrides, global_dirs, global_objs):
    dir_varnames, obj_varnames, fileparts = zip(*per_source(source_dirs, obj_dir, build_id))

    dir_varnames = ' '.join('$({})'.format(v) for v in dir_varnames)
    obj_varnames = ' '.join('$({})'.format(v) for v in obj_varnames)

    yield from fileparts

    # Set flags
    yield from ('{}: {} = {}'.format(obj_varnames, k, v) for k,v in part_overrides.items() if v)
    yield from ('{}: {} += {}'.format(obj_varnames, x, y) for x,y in itertools.chain(*map(lambda kv: zip(itertools.repeat(kv[0]), kv[1]), part_opts.items())))

    # Assign objects as dependencies
    yield '{}: {}'.format(executable, obj_varnames)
    yield '{} += {}'.format(global_dirs, dir_varnames)
    yield '{} += {}'.format(global_objs, obj_varnames)

def executable_opts(obj_root, build_id, executable, source_dirs, opts, overrides):
    dest_dir = os.path.normpath(os.path.join(obj_root, build_id))

    # Add compiler flags
    local_opts = extend_each(opts, {'CPPFLAGS': ('-I'+os.path.join(dest_dir, 'inc'),)})

    yield from ('', '######', '# Build ID: ' + build_id, '######', '')
    yield from make_part(source_dirs, os.path.join(dest_dir, 'obj'), build_id, executable, local_opts, overrides, 'build_dirs', 'build_objs')
    yield '{}: | {}'.format(executable, os.path.split(executable)[0])
    yield 'build_dirs += {}'.format(os.path.split(executable)[0])
    yield 'executable_name += {}'.format(executable)

def module_opts(source_dirs, obj_dir, build_id, name, opts, exe):
    build_dir = os.path.normpath(os.path.join(obj_dir, build_id))
    dest_dir = os.path.normpath(os.path.join(build_dir, name))

    local_opts = extend_each(opts, {'CPPFLAGS': (*('-I'+s for s in source_dirs), '-I'+os.path.join(build_dir, 'inc'), '-include {}.inc'.format(name))})

    yield from ('', '###', '# Build ID: ' + build_id, '# Module: ' + name, '# Source: ' + source_dirs[0], '# Destination: ' + dest_dir, '###', '')
    yield from make_part(source_dirs, dest_dir, build_id+'_'+name, exe, local_opts, {}, 'module_dirs', 'module_objs')

def get_makefile_lines(objdir, build_id, executable, source_dirs, module_info, config_file):
    global_opts = {k:config_file.get(k,[]) for k in ('CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')}
    global_overrides = {k:config_file.get(k,'') for k in ('CXX',)}

    yield from itertools.chain(
        executable_opts(objdir, build_id, executable, source_dirs, global_opts, global_overrides),
        *(module_opts((v['fname'],), objdir, build_id, k, extend_each(global_opts, v['opts']), executable) for k,v in module_info.items())
    )

