import itertools, operator
import os

def module_make(directory, opts):
    retval = '{0}/%.o: CPPFLAGS += -I{0}\n'.format(directory)

    for opt in opts:
        retval += '{0}/%.o: CXXFLAGS += {1}\n'.format(directory, opt)

    for base,dirs,files in os.walk(directory):
        for f in files:
            if os.path.splitext(f)[1] in ('.c',):
                retval += 'csrc += ' + os.path.join(base,f) + '\n'
            if os.path.splitext(f)[1] in ('.cc',):
                retval += 'cppsrc += ' + os.path.join(base,f) + '\n'

    return retval

def get_makefile_string(module_info, **config_file):
    retval = ''

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'

    if 'executable_name' in config_file:
        retval += 'executable_name ?= ' + config_file['executable_name'] + '\n\n'

    retval += 'module_dirs = ' + ' '.join(map(operator.itemgetter('fname'), module_info)) + '\n\n'
    retval += '\n'.join(module_make(v['fname'], v['opts']) for v in module_info)
    retval += '\n'

    return retval

