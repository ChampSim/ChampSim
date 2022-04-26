from .module_branch import *
from .module_btb import *
from .module_repl import *
from .module_pref import *

def get_module_name(path):
    fname_translation_table = str.maketrans('./-','_DH')
    return path.translate(fname_translation_table)

