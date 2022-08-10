import itertools, operator
import os

from . import util

def module_opts(source_dir, dest_dir, opts, exe):
    retval = '{}/%.o: CPPFLAGS += -I{}\n'.format(dest_dir, source_dir)
    retval += '{}/%.o: CPPFLAGS += -I{}\n'.format(dest_dir, dest_dir)
    retval += '{}/%.o: CPPFLAGS += -I{}\n'.format(dest_dir, os.path.split(dest_dir)[0])

    for opt in opts:
        retval += '{}/%.o: CXXFLAGS += {}\n'.format(dest_dir, opt)

    retval += '{}/%.o: {}/%.cc\n\tmkdir -p $(dir $@)\n\t$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<\n\n'.format(dest_dir, source_dir)

    retval += exe + ': module_dirs += ' + dest_dir + '\n'
    for base,dirs,files in os.walk(source_dir):
        for f,ext in map(os.path.splitext, files):
            if ext in ('.cc',):
                retval += exe + ': ' + os.path.join(dest_dir,f) + '.o\n'

    return retval

def get_makefile_string(genfile_dir, module_info, **config_file):
    retval = ''

    for k in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LDLIBS'):
        if k in config_file:
            retval += k + ' += ' + config_file[k] + '\n'

    executable = config_file.get('executable_name', 'bin/champsim')

    retval += 'executable_name += ' + executable + '\n'
    retval += executable + ': module_dirs += ' + genfile_dir + '\n'

    retval += '\n'.join(module_opts(v['fname'], os.path.join(genfile_dir, k), v['opts'], executable) for k,v in module_info.items())

    return retval

