import itertools
import functools
import os

from . import cxx
from . import modules

def mangled_declaration(fname, args, rtype, module_data):
    ''' Generate C++ code giving the mangled module specialization functions. '''
    argstring = ', '.join(a[0] for a in args)
    return f'{rtype} {module_data["func_map"][fname]}({argstring});'

def discriminator(full_variant_data, module_data):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    intern_name_map = {
        'branch_predictor': 'intern_core_',
        'btb': 'intern_core_',
        'prefetcher': 'intern_cache_',
        'replacement': 'intern_cache_'
    }
    discriminator_classname = module_data['class'].split('::')[-1]
    body = itertools.chain(
        ('using champsim::modules::legacy_module::legacy_module;',),
        *(cxx.function(fname, [
            f'if constexpr (kind == champsim::module::kind::{module_kind}) {{',
            f'  return {intern_name_map[module_kind]}->{module_data["func_map"][fname]}({", ".join(a[1] for a in args)});',
            '}'
        ], args=args) for module_kind,(fname,args,_) in full_variant_data)
    )
    yield 'template <champsim::modules::kind kind>'
    yield from cxx.struct(discriminator_classname, body, superclass='champsim::modules::legacy_module')
    yield ''

def get_discriminator(module_data):
    func = functools.partial(discriminator, itertools.chain(
        zip(itertools.repeat('branch_predictor'), modules.branch_variant_data),
        zip(itertools.repeat('btb'), modules.btb_variant_data),
        zip(itertools.repeat('prefetcher'), modules.pref_branch_variant_data),
        zip(itertools.repeat('prefetcher'), modules.pref_nonbranch_variant_data),
        zip(itertools.repeat('replacement'), modules.repl_variant_data)
    ))

    yield from itertools.chain.from_iterable(map(func, module_data))

def get_legacy_module_lines(branch_data, btb_data, pref_data, repl_data):
    '''
    Create three generators:
      - The first generates C++ code declaring all functions for the O3_CPU modules,
      - The second generates C++ code declaring all functions for the CACHE modules,
      - The third generates C++ code defining the functions.
    '''
    return (
        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.branch_variant_data, branch_data),
            itertools.product(modules.btb_variant_data, btb_data)
        )),

        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.pref_nonbranch_variant_data + modules.pref_branch_variant_data, pref_data),
            itertools.product(modules.repl_variant_data, repl_data)
        )),

        get_discriminator(itertools.chain(branch_data, btb_data, pref_data, repl_data))
       )

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser('Legacy module support generator')
    parser.add_argument('--prefix', required=True)
    parser.add_argument('legacyfiles', action='append')
    main_args = parser.parse_args()

    paths = map(os.path.dirname, main_args.legacyfiles)
    infos = [{
        'name': modules.get_module_name(p),
        'path': p,
        'legacy': True,
        'class': f'champsim::modules::generated::{p}'
    } for p in paths]

    infos = map(modules.get_branch_data, infos)
    infos = map(modules.get_btb_data, infos)
    infos = map(modules.get_pref_data, infos)
    infos = map(modules.get_repl_data, infos)
    infos = list(infos)

    with open(os.path.join(main_args.prefix, 'ooo_cpu_module_decl.inc'), 'wt') as wfp:
        print('\n'.join(mangled_declaration(*var, data) for var,data in itertools.chain(
                itertools.product(modules.branch_variant_data, infos),
                itertools.product(modules.btb_variant_data, infos)
        )), file=wfp)

    with open(os.path.join(main_args.prefix, 'cache_module_decl.inc'), 'wt') as wfp:
        print('\n'.join(mangled_declaration(*var, data) for var,data in itertools.chain(
                itertools.product(modules.pref_nonbranch_variant_data + modules.pref_branch_variant_data, infos),
                itertools.product(modules.repl_variant_data, infos)
        )), file=wfp)

    with open(os.path.join(main_args.prefix, 'module_def.inc'), 'wt') as wfp:
        print('\n'.join((
                '#ifndef GENERATED_MODULES_INC',
                '#define GENERATED_MODULES_INC',
                '#include "modules.h"',
                'namespace champsim::modules::generated',
                '{',
                *get_discriminator(infos),
                '}',
                '#endif'
        )), file=wfp)
