import itertools, operator

def module_make(module_name, directory, opts):
    retval = '{0}/%.o: CPPFLAGS += -I{0}\n'.format(directory)

    retval += '{0}/%.o: CXXFLAGS += {1}\n'.format(directory, opts)

    retval += 'obj/{0}: $(patsubst %.cc,%.o,$(wildcard {1}/*.cc)) $(patsubst %.c,%.o,$(wildcard {1}/*.c))\n'.format(module_name, directory)

    retval += '\t@mkdir -p $(dir $@)\n\tar -rcs $@ $^\n'

    return retval

def get_makefile_string(constants_header_name, instantiation_file_name, libfilenames, **config_file):
    retval = ''

    objdirs = tuple(['obj', *map(operator.itemgetter(0), libfilenames.values())])

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'
    retval += '\n'
    retval += 'exec : ' + config_file['executable_name'] + '\n\n'
    retval += 'clean: \n'
    retval += '\t$(RM) ' + constants_header_name + '\n'
    retval += '\t$(RM) ' + instantiation_file_name + '\n'
    retval += '\t$(RM) ' + 'inc/cache_modules.inc' + '\n'
    retval += '\t$(RM) ' + 'inc/ooo_cpu_modules.inc' + '\n'
    retval += '\n'.join('\t find {} -name \*.{} -delete'.format(v,suffix) for suffix,v in itertools.product(('o','d'), objdirs))
    retval += '\n\n'
    retval += config_file['executable_name'] + ': $(patsubst %.cc,%.o,$(wildcard src/*.cc)) ' + ' '.join('obj/' + k for k in libfilenames) + '\n'
    retval += '\t$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)\n\n'

    retval += '\n'.join(module_make(k, *v) for k,v in libfilenames.items())

    retval += '-include $(wildcard src/*.d)\n'
    retval += '\n'. join('-include $(wildcard {0}/*.d)'.format(*v) for v in libfilenames.values())
    retval += '\n'

    return retval

