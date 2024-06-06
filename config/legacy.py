import itertools
import functools
import os

from . import util
from . import cxx
from . import modules
from . import filewrite

branch_variant_data = [
    ('initialize_branch_predictor', tuple(), 'void'),
    ('last_branch_result', (('uint64_t', 'ip'), ('uint64_t', 'target'), ('uint8_t', 'taken'), ('uint8_t', 'branch_type')), 'void'),
    ('predict_branch', (('uint64_t','ip'),), 'uint8_t')
]
def get_branch_data(module_data):
    func_map = { v[0]: f'b_{module_data["name"]}_{v[0]}' for v in branch_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

btb_variant_data = [
    ('initialize_btb', tuple(), 'void'),
    ('update_btb', (('uint64_t','ip'), ('uint64_t','predicted_target'), ('uint8_t','taken'), ('uint8_t','branch_type')), 'void'),
    ('btb_prediction', (('uint64_t','ip'),), 'std::pair<uint64_t, uint8_t>')
]
def get_btb_data(module_data):
    func_map = { v[0]: f't_{module_data["name"]}_{v[0]}' for v in btb_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

pref_variant_data = [
    ('prefetcher_initialize', tuple(), 'void'),
    ('prefetcher_cache_operate', (('uint64_t', 'addr'), ('uint64_t', 'ip'), ('uint8_t', 'cache_hit'), ('bool', 'useful_prefetch'), ('uint8_t', 'type'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cache_fill', (('uint64_t', 'addr'), ('uint32_t', 'set'), ('uint32_t', 'way'), ('uint8_t', 'prefetch'), ('uint64_t', 'evicted_addr'), ('uint32_t', 'metadata_in')), 'uint32_t'),
    ('prefetcher_cycle_operate', tuple(), 'void'),
    ('prefetcher_final_stats', tuple(), 'void'),
    ('prefetcher_branch_operate', (('uint64_t', 'ip'), ('uint8_t', 'branch_type'), ('uint64_t', 'branch_target')), 'void')
]
def get_pref_data(module_data):
    prefix = 'pref'
    func_map = { v[0]: f'{prefix}_{module_data["name"]}_{v[0]}' for v in pref_variant_data }

    return util.chain(module_data,
        { 'func_map' : {
                **func_map,
                'l1i_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l1d_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l2c_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'llc_prefetcher_initialize': '_'.join((prefix, module_data['name'], 'prefetcher_initialize')),
                'l1i_prefetcher_cache_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l1d_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l2c_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'llc_prefetcher_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cache_operate')),
                'l1i_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l1d_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l2c_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'llc_prefetcher_cache_fill': '_'.join((prefix, module_data['name'], 'prefetcher_cache_fill')),
                'l1i_prefetcher_cycle_operate': '_'.join((prefix, module_data['name'], 'prefetcher_cycle_operate')),
                'l1i_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l1d_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l2c_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'llc_prefetcher_final_stats': '_'.join((prefix, module_data['name'], 'prefetcher_final_stats')),
                'l1i_prefetcher_branch_operate': '_'.join((prefix, module_data['name'], 'prefetcher_branch_operate'))
            }
        }
    )

repl_variant_data = [
    ('initialize_replacement', tuple(), 'void'),
    ('find_victim', (('uint32_t','triggering_cpu'), ('uint64_t','instr_id'), ('uint32_t','set'), ('const CACHE::BLOCK*','current_set'), ('uint64_t','ip'), ('uint64_t','full_addr'), ('uint32_t','type')), 'uint32_t'),
    ('update_replacement_state', (('uint32_t','triggering_cpu'), ('uint32_t','set'), ('uint32_t','way'), ('uint64_t','full_addr'), ('uint64_t','ip'), ('uint64_t','victim_addr'), ('uint32_t','type'), ('uint8_t','hit')), 'void'),
    ('replacement_final_stats', tuple(), 'void')
]
def get_repl_data(module_data):
    func_map = { v[0]: f'r_{module_data["name"]}_{v[0]}' for v in repl_variant_data }
    return util.chain(module_data, { 'func_map': func_map })

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

def variant_declaration(variant_data, module_data, classname):
    ''' Generate C++ code giving the mangled module specialization functions. '''
    discriminator_classname = module_data['class'].split('::')[-1]
    body = [
        'private:',
        'constexpr static bool has_function(std::string_view name);',
        'public:',
        f'using {classname}::{classname};',
        *(f'{rtype} {fname}({", ".join(a[0] for a in args)});' for fname, args, rtype in variant_data)
    ]
    yield from cxx.struct(discriminator_classname, body, superclass=classname)

def variant_function_body(fname, args, rtype, module_data):
    argnamestring = ', '.join(a[1] for a in args)
    dequalified_name = fname.split('::')[-1]
    mangled_name = module_data['func_map'][dequalified_name]
    body = [
        f'if constexpr (has_function("{mangled_name}")) {{',
        f'  return intern_->{mangled_name}({argnamestring});',
        '}',
    ]
    yield from cxx.function(fname, body, rtype=rtype, args=args)

def get_discriminator(variant_data, module_data):
    ''' For a given module function, generate C++ code defining the discriminator struct. '''
    classname = module_data['class']
    yield 'constexpr'
    yield from cxx.function(f'{classname}::has_function',['return std::string_view{CHAMPSIM_LEGACY_FUNCTION_NAMES}.find(name) != std::string_view::npos;'], rtype='bool', args=(('std::string_view','name'),))

    for fname, args, rtype in variant_data:
        yield ''
        yield from variant_function_body(f'{classname}::{fname}', args, rtype, module_data)

def apply_getfunction(info):
    return {
        'branch': get_branch_data,
        'btb': get_btb_data,
        'prefetcher': get_pref_data,
        'replacement': get_repl_data
    }.get(info['type_guess'], lambda x: x)(info)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser('Legacy module support generator')
    parser.add_argument('--kind', choices=['options','header','mangle','source'])
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
    infos = list(map(apply_getfunction, infos))

    parts = {
        'branch': ('ooo_cpu.h', 'branch_predictor', branch_variant_data),
        'btb': ('ooo_cpu.h', 'btb', btb_variant_data),
        'prefetcher': ('cache.h', 'prefetcher', pref_variant_data),
        'replacement': ('cache.h', 'replacement', repl_variant_data)
    }
    zipped_parts = ((parts.get(i['type_guess'], ('', '', {})),i) for i in infos)

    fileparts = []

    if args.kind == 'options':
        fileparts.extend(
            (os.path.join(mod_info['path'], 'legacy.options'), get_legacy_module_opts_lines(mod_info))
        for mod_info in infos)

    if args.kind == 'mangle':
        fileparts.extend((os.path.join(mod_info['path'], 'legacy_bridge.inc'), filewrite.cxx_file((
            f'#ifndef CHAMPSIM_LEGACY_{mod_info["name"]}',
            f'#define CHAMPSIM_LEGACY_{mod_info["name"]}',
            *(mangled_declaration(*v, mod_info) for v in var),
            '#endif'
        ))) for (_, _, var), mod_info in zipped_parts)

    if args.kind == 'header':
        fileparts.extend((os.path.join(mod_info['path'], 'legacy_bridge.h'), filewrite.cxx_file((
            '#include <string_view>',
            '#include "modules.h"',
            f'#include "{header_name}"', '',
            'namespace champsim::modules::generated',
            '{',
            *variant_declaration(variant, mod_info, classname),
            '}'
        ))) for (header_name, classname, variant), mod_info in zipped_parts)

    if args.kind == 'source':
        fileparts.extend((os.path.join(mod_info['path'], 'legacy_bridge.cc'), filewrite.cxx_file((
            '#include "legacy_bridge.h"', '',
            *get_discriminator(variant, mod_info),
        ))) for (header_name, _, variant), mod_info in zipped_parts)


    for fname, fcontents in fileparts:
        with open(fname, 'wt') as wfp:
            for line in fcontents:
                print(line, file=wfp)
