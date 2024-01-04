import itertools
import functools
import os

from . import util
from . import cxx
from . import modules

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

def get_discriminator(full_variant_data, module_data):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    intern_name_map = {
        'branch_predictor': 'intern_core_',
        'btb': 'intern_core_',
        'prefetcher': 'intern_cache_',
        'replacement': 'intern_cache_'
    }
    discriminator_classname = module_data['class'].split('::')[-1]
    body = itertools.chain(
        (f'using champsim::modules::legacy_module::legacy_module;',),
        *(cxx.function(fname, [
            f'if constexpr (kind == champsim::module::kind::{module_kind}) {{',
            f'  return {intern_name_map[kind]}->{module_data["func_map"][fname]}({", ".join(a[1] for a in args)});',
            '}'
        ], args=args) for kind,(fname,args,_) in full_variant_data)
    )
    yield 'template <champsim::modules::kind kind>'
    yield from cxx.struct(discriminator_classname, body, superclass='champsim::modules::legacy_module')
    yield ''

def get_legacy_module_lines(branch_data, btb_data, pref_data, repl_data):
    '''
    Create three generators:
      - The first generates C++ code declaring all functions for the O3_CPU modules,
      - The second generates C++ code declaring all functions for the CACHE modules,
      - The third generates C++ code defining the functions.
    '''
    discriminator = functools.partial(get_discriminator, itertools.chain(
        zip(itertools.repeat('branch_predictor'), modules.branch_variant_data),
        zip(itertools.repeat('btb'), modules.btb_variant_data),
        zip(itertools.repeat('prefetcher'), modules.pref_branch_variant_data),
        zip(itertools.repeat('prefetcher'), modules.pref_nonbranch_variant_data),
        zip(itertools.repeat('replacement'), modules.repl_variant_data)
    ))
    return (
        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.branch_variant_data, branch_data),
            itertools.product(modules.btb_variant_data, btb_data)
        )),

        (mangled_declaration(*var, data) for var,data in itertools.chain(
            itertools.product(modules.pref_nonbranch_variant_data + modules.pref_branch_variant_data, pref_data),
            itertools.product(modules.repl_variant_data, repl_data)
        )),

        map(discriminator, itertools.chain(branch_data, btb_data, pref_data, repl_data))
       )

def generate_module_information(containing_dir, module_info):
    ''' Generates all of the include-files with module information '''
    if any(module_info.values()):
        core_declarations, cache_declarations, module_definitions = get_legacy_module_lines(
                module_info['branch'].values(),
                module_info['btb'].values(),
                module_info['pref'].values(),
                module_info['repl'].values()
            )

        yield os.path.join(containing_dir, 'ooo_cpu_module_decl.inc'), cxx_file(core_declarations)
        yield os.path.join(containing_dir, 'cache_module_decl.inc'), cxx_file(cache_declarations)
        yield os.path.join(containing_dir, 'module_def.inc'), cxx_file((
                '#ifndef GENERATED_MODULES_INC',
                '#define GENERATED_MODULES_INC',
                '#include "modules.h"',
                'namespace champsim::modules::generated',
                '{',
                *module_definitions,
                '}',
                '#endif'
        ))

        joined_info_items = itertools.chain(*(v.items() for v in module_info.values()))
        for k,v in joined_info_items:
            fname = os.path.join(containing_dir, k+'.options')
            yield fname, get_legacy_module_opts_lines(v)


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser('Legacy module support generator')
    parser.add_argument('--prefix', required=True)
    parser.add_argument('legacyfiles', action='append')
    args = parser.parse_args()

    paths = map(os.path.dirname, args.legacyfiles)
    infos = [{
        'name': modules.get_module_name(p),
        'path': p,
        'legacy': True,
        'class': f'champsim::modules::generated::{p}'
    } for p in paths]

    for fname, fcontents in itertools.islice(generate_module_information(args.prefix, infos), 3):
        with open(fname, 'wt') as wfp:
            print('\n'.join(fcontents), file=wfp)


