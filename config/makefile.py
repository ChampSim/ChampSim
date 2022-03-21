import itertools, operator
import os

def module_make(directory, opts):
    retval = '{0}/%.o: CPPFLAGS += -I{0}\n'.format(directory)

    for opt in opts.split():
        retval += '{0}/%.o: CXXFLAGS += {1}\n'.format(directory, opt)

    for base,dirs,files in os.walk(directory):
        for f in files:
            if os.path.splitext(f)[1] in ('.c',):
                retval += 'csrc += ' + os.path.join(base,f) + '\n'
            if os.path.splitext(f)[1] in ('.cc',):
                retval += 'cppsrc += ' + os.path.join(base,f) + '\n'

    return retval

def get_makefile_string(constants_header_name, instantiation_file_name, libfilenames, **config_file):
    retval = ''

    objdirs = tuple(['obj', *map(operator.itemgetter(0), libfilenames.values())])

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'
    retval += '\n'
    retval += 'executable_name ?= ' + config_file['executable_name'] + '\n\n'
    retval += 'exec : $(executable_name)\n\n'
    retval += 'clean: \n'
    retval += '\t$(RM) ' + constants_header_name + '\n'
    retval += '\t$(RM) ' + instantiation_file_name + '\n'
    retval += '\t$(RM) ' + 'inc/cache_modules.inc' + '\n'
    retval += '\t$(RM) ' + 'inc/ooo_cpu_modules.inc' + '\n'
    retval += '\n'.join('\t find {} -name \*.{} -delete'.format(v,suffix) for suffix,v in itertools.product(('o','d'), objdirs))
    retval += '\n\n'

    retval += '\n'.join(module_make(*v) for v in libfilenames.values())

    retval += '\n'
    retval += '-include $(wildcard src/*.d)\n'
    retval += '\n'. join('-include $(wildcard {0}/*.d)'.format(*v) for v in libfilenames.values())
    retval += '\n'

    return retval

