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
    retval += 'required_dirs += ' + ' '.join(generate_dirs(os.path.split(executable)[0])) + '\n'
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

    retval += dir_varname + ' = ' + ' '.join(generate_dirs(dest_dir)) + '\n'
    for base,dirs,files in os.walk(source_dir):
        for f,ext in map(os.path.splitext, files):
            if ext in ('.cc',):
                retval += obj_varname + ' += ' + os.path.join(dest_dir,f) + '.o\n'

    retval += '$({}): | $({})\n'.format(obj_varname, dir_varname)
    retval += '$({}): CPPFLAGS += -I{}\n'.format(obj_varname, source_dir)

    for opt in opts:
        retval += '$({}): CXXFLAGS += {}\n'.format(obj_varname, opt)

    retval += '$({}): {}/%.o: {}/%.cc\n'.format(obj_varname, dest_dir, source_dir)

    retval += exe + ': $(' + obj_varname + ')\n'
    retval += 'module_objs += $(' + obj_varname + ')\n'
    retval += 'required_dirs += $(' + dir_varname + ')\n'

    return retval

def get_makefile_string(build_id, module_info, **config_file):

    name = config_file.get('name')
    executable = '$(bindir)/' + config_file.get('executable_name', 'champsim' + ('' if name is None else '_'+name))

    retval = executable_opts('$(objdir)', build_id, executable, config_file)
    retval += '\n'
    retval += '\n'.join(module_opts(v['fname'], '$(objdir)', build_id, k, v['opts'], executable) for k,v in module_info.items())

    return retval

