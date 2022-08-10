import itertools, operator
import os

from . import util

def module_opts(directory, opts):
    retval = '{}/%.o: CPPFLAGS += -I{}\n'.format(directory, directory)

    for opt in opts:
        retval += '{}/%.o: CXXFLAGS += {}\n'.format(directory, opt)

    return retval;

def module_sources(directory, exes):
    retval = ''

    retval += '{}/%.o: {}/%.cc\n'.format(directory, directory)
    for base,dirs,files in os.walk(directory):
        for f,ext in map(os.path.splitext, files):
            if ext in ('.cc',):
                retval += ' '.join(exes) + ': ' + os.path.join(base,f) + '.o\n'

    return retval

def get_makefile_string(module_info, **config_file):
    retval = ''

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'

    executables = util.wrap_list(config_file.get('executable_name', 'bin/champsim'))

    retval += 'executable_name = ' + ' '.join(executables) + '\n\n'

    retval += 'module_dirs = ' + ' '.join(map(operator.itemgetter('fname'), module_info)) + '\n\n'
    retval += '\n'.join(module_opts(v['fname'], v['opts']) for v in module_info)
    retval += '\n'
    retval += '\n'.join(module_sources(v['fname'], executables) for v in module_info)
    retval += '\n'

    return retval

