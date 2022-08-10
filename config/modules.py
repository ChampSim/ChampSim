from .module_branch import *
from .module_btb import *
from .module_repl import *
from .module_pref import *

import os

def get_module_name(path):
    fname_translation_table = str.maketrans('./-','_DH')
    return path.translate(fname_translation_table)

# Get the paths to built-in modules
def default_modules(dirname):
    return tuple(os.path.join(dirname, d) for d in os.listdir(dirname) if os.path.isdir(os.path.join(dirname, d)))

# Try the built-in module directories, then try to interpret as a path
def default_dir(dirname, f):
    fname = os.path.join(dirname, f)
    if not os.path.exists(fname):
        fname = os.path.relpath(os.path.expandvars(os.path.expanduser(f)))
    if not os.path.exists(fname):
        print('Path "' + fname + '" does not exist. Exiting...')
        sys.exit(1)
    return fname

