import os
import itertools

from . import util

from .module_branch import *
from .module_btb import *
from .module_repl import *
from .module_pref import *

def get_module_name(path):
    fname_translation_table = str.maketrans('./-','_DH')
    return path.translate(fname_translation_table)

# Get the paths to built-in modules
def default_modules(dirname):
    files = (os.path.join(dirname, d) for d in os.listdir(dirname))
    files = filter(os.path.isdir, files)
    yield from ({'name': get_module_name(f), 'fname': f, '_is_instruction_prefetcher': f.endswith('_instr')} for f in files)

# Try the built-in module directories, then try to interpret as a path
def default_dir(dirname, f):
    fname = os.path.join(dirname, f)
    if not os.path.exists(fname):
        fname = os.path.relpath(os.path.expandvars(os.path.expanduser(f)))
    if not os.path.exists(fname):
        print('[WARNING]', 'Path "' + fname + '" does not exist.')
        return None
    return fname

def get_module_data(names_key, paths_key, values, directory, get_func):
    namekey_pairs = itertools.chain(*(zip(c[names_key], c[paths_key], itertools.repeat(c.get('_is_instruction_prefetcher', False))) for c in values))
    data = util.combine_named(
        default_modules(directory),
        ({'name': name, 'fname': path, '_is_instruction_prefetcher': is_instr} for name,path,is_instr in namekey_pairs)
        )
    return {k: util.chain((get_func(k,v['_is_instruction_prefetcher']) if v['_is_instruction_prefetcher'] else get_func(k)), v) for k,v in data.items()}

