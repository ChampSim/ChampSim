import itertools, operator
import os

def walk_to(source_dir, dest_dir, extensions=('.cc',)):
    for base,dirs,files in os.walk(source_dir):
        reldir = os.path.join(dest_dir, os.path.relpath(base, source_dir))
        yield reldir, [os.path.normpath(os.path.join(reldir, f)+'.o') for f,ext in map(os.path.splitext, files) if ext in extensions]

def extend_each(x,y):
    merges = {k: (*x[k],*y[k]) for k in x if k in y}
    return {**x, **y, **merges}

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
        objs = linejoin(obj_filenames)

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

    retval = '\n'.join(fileparts)

    # Set flags
    for k,v in part_overrides.items():
        if v:
            retval += '{}: {} = {}\n'.format(obj_varnames, k, v)
    for k,v in part_opts.items():
        for x in v:
            retval += '{}: {} += {}\n'.format(obj_varnames, k, x)

    # Assign objects as dependencies
    retval += '{}: {}\n'.format(executable, obj_varnames)
    retval += '{} += {}\n'.format(global_dirs, dir_varnames)
    retval += '{} += {}\n'.format(global_objs, obj_varnames)

    return retval

def executable_opts(obj_root, build_id, executable, source_dirs, opts, overrides):
    dest_dir = os.path.normpath(os.path.join(obj_root, build_id))

    # Add compiler flags
    local_opts = extend_each(opts, {'CPPFLAGS': ('-I'+os.path.join(dest_dir, 'inc'),)})

    retval = '######\n'
    retval += '# Build ID: ' + build_id + '\n'
    retval += '######\n\n'

    retval += make_part(source_dirs, os.path.join(dest_dir, 'obj'), build_id, executable, local_opts, overrides, 'build_dirs', 'build_objs')
    retval += '\n'
    retval += '{}: | {}\n'.format(executable, os.path.split(executable)[0])
    retval += 'build_dirs += {}\n'.format(os.path.split(executable)[0])
    retval += 'executable_name += {}\n'.format(executable)
    return retval

def module_opts(source_dirs, obj_dir, build_id, name, opts, exe):
    build_dir = os.path.normpath(os.path.join(obj_dir, build_id))
    dest_dir = os.path.normpath(os.path.join(build_dir, name))

    local_opts = extend_each(opts, {'CPPFLAGS': (*('-I'+s for s in source_dirs), '-I'+os.path.join(build_dir, 'inc'), '-include {}.inc'.format(name))})

    retval = '###\n'
    retval += '# Build ID: ' + build_id + '\n'
    retval += '# Module: ' + name + '\n'
    retval += '# Source: ' + source_dirs[0] + '\n'
    retval += '# Destination: ' + dest_dir + '\n'
    retval += '###\n\n'

    retval += make_part(source_dirs, dest_dir, build_id+'_'+name, exe, local_opts, {}, 'module_dirs', 'module_objs')

    return retval

def get_makefile_string(objdir, build_id, executable, source_dirs, module_info, config_file):
    global_opts = {k:config_file.get(k,[]) for k in ('CPPFLAGS', 'CXXFLAGS', 'LDFLAGS', 'LDLIBS')}
    global_overrides = {k:config_file.get(k,'') for k in ('CXX',)}

    retval = executable_opts(objdir, build_id, executable, source_dirs, global_opts, global_overrides)
    retval += '\n'
    retval += '\n'.join(module_opts((v['fname'],), objdir, build_id, k, extend_each(global_opts, v['opts']), executable) for k,v in module_info.items())

    return retval

