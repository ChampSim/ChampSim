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
    mangled_name = module_data['func_map'][fname]
    body = [
        f'if constexpr (has_function("{mangled_name}"sv)) {{',
        f'  return intern_->{mangled_name}({argnamestring});',
        '}',
    ]
    yield ''
    yield from cxx.function(fname, body, args=args)

def get_discriminator(variant_data, module_data, classname):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    discriminator_classname = module_data['class'].split('::')[-1]
    body = itertools.chain(
        ('private:',),
        ('constexpr static',*cxx.function('has_function',['return "CHAMPSIM_LEGACY_FUNCTION_NAMES"sv.find(name);'], args=(('std::string_view','name'),)),''),
        ('public:',),
        (f'using {classname}::{classname};',),
        *(variant_function_body(n,a,module_data) for n,a,_ in variant_data)
    )
    yield from cxx.struct(discriminator_classname, body, superclass=classname)
    yield ''

def get_bridge(header_name, discrim, variant, mod_info):
    yield os.path.join(mod_info['path'], 'legacy_bridge.cc'), filewrite.cxx_file((
        '#include <string_view>',
        '#include "modules.h"',
        f'#include "{header_name}"', '',
        'using namespace std::literals::string_view_literals;', '',
        'namespace champsim::modules::generated',
        '{',
        *discrim(mod_info),
        '}'
    ))

    yield os.path.join(mod_info['path'], 'legacy_bridge.h'), filewrite.cxx_file((
        f'#ifndef CHAMPSIM_LEGACY_{mod_info["name"]}',
        f'#define CHAMPSIM_LEGACY_{mod_info["name"]}',
        *(mangled_declaration(*var, mod_info) for var in variant),
        '#endif'
    ))

    fname = os.path.join(mod_info['path'], 'legacy.options')
    yield fname, get_legacy_module_opts_lines(mod_info)

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

    #yield from itertools.chain.from_iterable(itertools.starmap(functools.partial(get_bridge, 'ooo_cpu.h'), core_bridge_params))
    #yield from itertools.chain.from_iterable(itertools.starmap(functools.partial(get_bridge, 'cache.h'), cache_bridge_params))

def generate_module_information(containing_dir, module_info):
    ''' Generates all of the include-files with module information '''
    return get_legacy_module_lines(
        containing_dir,
        module_info['branch'].values(),
        module_info['btb'].values(),
        module_info['pref'].values(),
        module_info['repl'].values()
    )

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser('Legacy module support generator')
    parser.add_argument('--kind', choices=['options','header','source'])
    parser.add_argument('paths', action='append')
    args = parser.parse_args()

    infos = [{
        'name': modules.get_module_name(p),
        'path': p,
        'legacy': True
    } for p in args.paths]
    for i in infos:
        i.update({
            'type_guess': next(filter(lambda t: t in i['path'], ('branch', 'btb', 'prefetcher', 'replacement')), ''),
            'class': f'champsim::modules::generated::{i["name"]}'
        })

    parts = {
        'branch': ('ooo_cpu.h', functools.partial(get_discriminator, modules.branch_variant_data, classname='branch_predictor'), modules.branch_variant_data, modules.get_branch_data),
        'btb': ('ooo_cpu.h', functools.partial(get_discriminator, modules.btb_variant_data, classname='btb'), modules.btb_variant_data, modules.get_btb_data),
        'prefetcher': ('cache.h', functools.partial(get_discriminator, modules.pref_branch_variant_data + modules.pref_nonbranch_variant_data, classname='prefetcher'), modules.pref_branch_variant_data + modules.pref_nonbranch_variant_data, modules.get_pref_data),
        'replacement': ('cache.h', functools.partial(get_discriminator, modules.repl_variant_data, classname='replacement'), modules.repl_variant_data, modules.get_repl_data)
    }
    zipped_parts = ((parts.get(i['type_guess'], ('', lambda _: tuple(), {},lambda x: x)),i) for i in infos)

    fileparts = []

    if args.kind == 'options':
        fileparts.extend(
            (os.path.join(mod_info['path'], 'legacy.options'), get_legacy_module_opts_lines(getfunc(mod_info)))
        for (_, _, _, getfunc), mod_info in zipped_parts)

    if args.kind == 'header':
        fileparts.extend((os.path.join(mod_info['path'], 'legacy_bridge.h'), filewrite.cxx_file((
            f'#ifndef CHAMPSIM_LEGACY_{mod_info["name"]}',
            f'#define CHAMPSIM_LEGACY_{mod_info["name"]}',
            *(mangled_declaration(*v, getfunc(mod_info)) for v in var),
            '#endif'
        ))) for (_, _, var, getfunc), mod_info in zipped_parts)

    if args.kind == 'source':
        fileparts.extend((os.path.join(mod_info['path'], 'legacy_bridge.cc'), filewrite.cxx_file((
            '#include <string_view>',
            '#include "modules.h"',
            f'#include "{header_name}"', '',
            'using namespace std::literals::string_view_literals;', '',
            'namespace champsim::modules::generated',
            '{',
            *discrim(getfunc(mod_info)),
            '}'
        ))) for (header_name, discrim, _, getfunc), mod_info in zipped_parts)

    filewrite.Fragment(fileparts).write()
