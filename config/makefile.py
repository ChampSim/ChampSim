import itertools, operator
import os

from . import util

def generate_dirs(path):
    head, tail = os.path.split(path)
    if head != '':
        yield from generate_dirs(head)
    yield os.path.join(head, tail)

def executable_opts(obj_dir, build_id, executable, config_file):
    dest_dir = os.path.join(obj_dir, build_id)

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
    retval += executable + ': | ' + ' '.join(generate_dirs(os.path.split(executable)[0])) + '\n'
    retval += 'build_dirs += ' + ' '.join(generate_dirs(os.path.split(executable)[0])) + '\n'
    return retval

def module_opts(source_dir, obj_dir, build_id, name, opts, exe):
    dest_dir = os.path.join(obj_dir, build_id, name)
    obj_varname = build_id + '_' + name + '_objs'
    dir_varname = build_id + '_' + name + '_dirs'

    retval = '###\n'
    retval += '# Build ID: ' + build_id + '\n'
    retval += '# Module: ' + name + '\n'
    retval += '# Source: ' + source_dir + '\n'
    retval += '# Destination: ' + dest_dir + '\n'
    retval += '###\n\n'

    obj_dirnames = [dest_dir]
    obj_filenames = []
    for base,dirs,files in os.walk(source_dir):
        obj_dirnames.extend(os.path.join(base, d) for d in dirs)
        obj_filenames.extend(os.path.join(dest_dir, os.path.relpath(base, source_dir), f)+'.o' for f,ext in map(os.path.splitext, files) if ext in ('.cc',))

    retval += '{} = {}\n'.format(dir_varname, ' '.join(obj_dirnames))
    for f in obj_filenames:
        retval += '{} += {}\n'.format(obj_varname, f)

    retval += '$({}): | $({})\n'.format(obj_varname, dir_varname)
    retval += '$({}): CPPFLAGS += -I{}\n'.format(obj_varname, source_dir)

    for opt in opts:
        retval += '$({}): CXXFLAGS += {}\n'.format(obj_varname, opt)

    retval += '$({}): {}/%.o: {}/%.cc\n'.format(obj_varname, dest_dir, source_dir)

    retval += exe + ': $(' + obj_varname + ')\n'
    retval += 'module_objs += $(' + obj_varname + ')\n'
    retval += 'module_dirs += $(' + dir_varname + ')\n'

    return retval

def get_makefile_string(objdir, build_id, executable, module_info, config_file):
    retval = executable_opts(objdir, build_id, executable, config_file)
    retval += '\n'
    retval += '\n'.join(module_opts(v['fname'], objdir, build_id, k, v['opts'], executable) for k,v in module_info.items())

    return retval

