import itertools
import functools
import os

from . import util
from . import cxx
from . import modules
from . import filewrite

def get_legacy_module_opts_lines(module_data):
    '''
    Generate an iterable of the compiler options for a particular module
    '''
    full_funcmap = util.chain(module_data['func_map'], module_data.get('deprecated_func_map', {}))
    yield from  (f'-D{k}={v}' for k,v in full_funcmap.items())

def mangled_declaration(fname, args, rtype, module_data):
    ''' Generate C++ code giving the mangled module specialization functions. '''
    argstring = ', '.join(a[0] for a in args)
    return f'{rtype} {module_data["func_map"][fname]}({argstring});'

def variant_function_body(fname, args, module_data):
    argnamestring = ', '.join(a[1] for a in args)
    body = [f'return intern_->{module_data["func_map"][fname]}({argnamestring});']
    yield from cxx.function(fname, body, args=args)
    yield ''

def get_discriminator(variant_data, module_data, classname):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    discriminator_classname = module_data['class'].split('::')[-1]
    body = itertools.chain(
        (f'using {classname}::{classname};',),
        *(variant_function_body(n,a,module_data) for n,a,_ in variant_data)
    )
    yield from cxx.struct(discriminator_classname, body, superclass=classname)
    yield ''

def get_legacy_module_lines(containing_dir, branch_data, btb_data, pref_data, repl_data):
    '''
    Create three generators:
      - The first generates C++ code declaring all functions for the O3_CPU modules,
      - The second generates C++ code declaring all functions for the CACHE modules,
      - The third generates C++ code defining the functions.
    '''
    branch_discriminator = functools.partial(get_discriminator, modules.branch_variant_data, classname='branch_predictor')
    btb_discriminator = functools.partial(get_discriminator, modules.btb_variant_data, classname='btb')
    repl_discriminator = functools.partial(get_discriminator, modules.repl_variant_data, classname='replacement')

    def pref_discriminator(v):
        local_branch_variant_data = modules.pref_branch_variant_data if v.get('_is_instruction_prefetcher') else []
        return get_discriminator([*modules.pref_nonbranch_variant_data, *local_branch_variant_data], v, classname='prefetcher')

    yield os.path.join(containing_dir, 'ooo_cpu_module_decl.inc'), filewrite.cxx_file(
        mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.branch_variant_data, branch_data),
            itertools.product(modules.btb_variant_data, btb_data)
        )
    )

    yield os.path.join(containing_dir, 'cache_module_decl.inc'), filewrite.cxx_file(
        mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.pref_nonbranch_variant_data + modules.pref_branch_variant_data, pref_data),
            itertools.product(modules.repl_variant_data, repl_data)
        )
    )

    core_bridge_params = itertools.chain(
        zip(itertools.repeat(branch_discriminator), branch_data),
        zip(itertools.repeat(btb_discriminator), btb_data),
    )

    for discrim, mod_info in core_bridge_params:
        yield os.path.join(mod_info['path'], 'legacy_bridge.cc'), filewrite.cxx_file((
            '#include "modules.h"',
            '#include "ooo_cpu.h"',
            'namespace champsim::modules::generated',
            '{',
            *discrim(mod_info),
            '}'
        ))

    cache_bridge_params = itertools.chain(
        zip(itertools.repeat(pref_discriminator), pref_data),
        zip(itertools.repeat(repl_discriminator), repl_data)
    )
    for discrim, mod_info in cache_bridge_params:
        yield os.path.join(mod_info['path'], 'legacy_bridge.cc'), filewrite.cxx_file((
            '#include "modules.h"',
            '#include "cache.h"',
            'namespace champsim::modules::generated',
            '{',
            *discrim(mod_info),
            '}'
        ))

    joined_info_items = itertools.chain(branch_data, btb_data, pref_data, repl_data)
    for v in joined_info_items:
        fname = os.path.join(v['path'], 'legacy.options')
        yield fname, get_legacy_module_opts_lines(v)

def generate_module_information(containing_dir, module_info):
    ''' Generates all of the include-files with module information '''
    yield from get_legacy_module_lines(
        containing_dir,
        module_info['branch'].values(),
        module_info['btb'].values(),
        module_info['pref'].values(),
        module_info['repl'].values()
    )

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser('Legacy module support generator')
    parser.add_argument('legacyfiles', action='append')
    args = parser.parse_args()

    paths = map(os.path.dirname, args.legacyfiles)
    infos = [{
        'name': modules.get_module_name(p),
        'path': p,
        'legacy': True,
        'class': f'champsim::modules::generated::{p}'
    } for p in paths]


