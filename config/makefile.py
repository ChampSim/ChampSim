import itertools, operator
import os

from . import util

def generate_dirs(path):
    head, tail = os.path.split(path)
    if head != '':
        yield from generate_dirs(head)
    yield os.path.join(head, tail)

def walk_to(source_dir, dest_dir, extensions=('.cc',)):
    obj_dirnames = [dest_dir]
    obj_filenames = []
    for base,dirs,files in os.walk(source_dir):
        obj_dirnames.extend(os.path.join(base, d) for d in dirs)
        obj_filenames.extend(os.path.normpath(os.path.join(dest_dir, os.path.relpath(base, source_dir), f)+'.o') for f,ext in map(os.path.splitext, files) if ext in extensions)

    return obj_dirnames, obj_filenames

def executable_opts(obj_root, build_id, executable, source_dirs, config_file):
    dest_dir = os.path.normpath(os.path.join(obj_root, build_id))
    obj_dir = os.path.join(dest_dir, 'obj')
    dir_varname = build_id + '_dirs'
    obj_varname = build_id + '_objs'

    retval = '######\n'
    retval += '# Build ID: ' + build_id + '\n'
    retval += '######\n\n'

    retval += 'executable_name += ' + executable + '\n'

    # Override the compiler
    for k in ('CC', 'CXX'):
        if k in config_file:
            retval += '{}: {} = {}\n'.format(executable, k, config_file[k])

    # Add compiler flags
    for k in ('CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += '{}: {} += {}\n'.format(executable, k, config_file[k])

    retval += executable + ': CPPFLAGS += -I' + os.path.join(dest_dir, 'inc') + '\n'

    for i, src_info in enumerate((*walk_to(src, obj_dir), src) for src in source_dirs):
        obj_dirnames, obj_filenames, src_dir = src_info

        local_dir_varname = dir_varname + '_' + str(i)
        local_obj_varname = obj_varname + '_' + str(i)

        retval += '{} = {}\n'.format(local_dir_varname, ' '.join(obj_dirnames))
        for f in obj_filenames:
            retval += '{} += {}\n'.format(local_obj_varname, f)

        retval += '$({}): {}/%.o: {}/%.cc | $({})\n'.format(local_obj_varname, obj_dir, src_dir, local_dir_varname)
        retval += '{}: $({}) | $({})\n'.format(executable, local_obj_varname, local_dir_varname)

        retval += 'build_objs += $({})\n'.format(local_obj_varname)
        retval += 'build_dirs += $({})\n'.format(local_dir_varname)

    retval += 'build_dirs += {}\n'.format(os.path.split(executable)[0])
    retval += '{}: | {}\n'.format(executable, os.path.split(executable)[0])
    return retval

def module_opts(source_dir, obj_dir, build_id, name, opts, exe):
    dest_dir = os.path.normpath(os.path.join(obj_dir, build_id, name))
    obj_varname = build_id + '_' + name + '_objs'
    dir_varname = build_id + '_' + name + '_dirs'

    retval = '###\n'
    retval += '# Build ID: ' + build_id + '\n'
    retval += '# Module: ' + name + '\n'
    retval += '# Source: ' + source_dir + '\n'
    retval += '# Destination: ' + dest_dir + '\n'
    retval += '###\n\n'

    obj_dirnames, obj_filenames = walk_to(source_dir, dest_dir)

    retval += '{} = {}\n'.format(dir_varname, ' '.join(obj_dirnames))
    for f in obj_filenames:
        retval += '{} += {}\n'.format(obj_varname, f)

    retval += '$({}): CPPFLAGS += -I{}\n'.format(obj_varname, source_dir)
    for opt in opts:
        retval += '$({}): CXXFLAGS += {}\n'.format(obj_varname, opt)

    retval += '$({}): {}/%.o: {}/%.cc | $({})\n'.format(obj_varname, dest_dir, source_dir, dir_varname)

    retval += exe + ': $(' + obj_varname + ')\n'
    retval += 'module_objs += $(' + obj_varname + ')\n'
    retval += 'module_dirs += $(' + dir_varname + ')\n'

    return retval

def get_makefile_string(objdir, build_id, executable, source_dirs, module_info, config_file):
    retval = executable_opts(objdir, build_id, executable, source_dirs, config_file)
    retval += '\n'
    retval += '\n'.join(module_opts(v['fname'], objdir, build_id, k, v['opts'], executable) for k,v in module_info.items())

    return retval

