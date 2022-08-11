import itertools, operator
import os

from . import util

def module_opts(source_dir, genfile_dir, name, opts, exe):
    dest_dir = os.path.join(genfile_dir, name)

    retval = '###\n'
    retval += '# Build ID: ' + os.path.split(genfile_dir)[1] + '\n'
    retval += '# Module: ' + name + '\n'
    retval += '###\n\n'

    varname = os.path.split(genfile_dir)[1] + '_' + name + '_objs'
    for base,dirs,files in os.walk(source_dir):
        for f,ext in map(os.path.splitext, files):
            if ext in ('.cc',):
                retval += varname + ' += ' + os.path.join(dest_dir,f) + '.o\n'

    retval += '$(' + varname + '): | ' + dest_dir + '\n'
    retval += '{}/%.o: CPPFLAGS += -I{}\n'.format(dest_dir, source_dir)
    retval += '{}/%.o: CPPFLAGS += -I{}\n'.format(dest_dir, dest_dir)

    for opt in opts:
        retval += '{}/%.o: CXXFLAGS += {}\n'.format(dest_dir, opt)

    retval += '{}/%.o: {}/%.cc\n\t$(COMPILE.cc) $(OUTPUT_OPTION) $<\n'.format(dest_dir, source_dir)
    retval += '{}:\n\t-mkdir $@\n\n'.format(dest_dir)

    retval += exe + ': $(' + varname + ')\n'

    return retval

def get_makefile_string(genfile_dir, module_info, **config_file):

    retval = '######\n'
    retval += '# Build ID: ' + os.path.split(genfile_dir)[1] + '\n'
    retval += '######\n\n'

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'

    executable = config_file.get('executable_name', 'bin/champsim')

    retval += 'executable_name += ' + executable + '\n'
    retval += executable + ': CPPFLAGS += -I' + genfile_dir + '\n'
    retval += os.path.split(executable)[0] + '/:\n\t-mkdir $@\n\n'

    retval += '\n'.join(module_opts(v['fname'], genfile_dir, k, v['opts'], executable) for k,v in module_info.items())

    return retval

