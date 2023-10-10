#    Copyright 2023 The ChampSim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import itertools
import functools
import operator
import os
import tempfile
import multiprocessing as mp

from . import util
from . import cxx

def channel_name(*, lower, upper):
    return f'{upper}_to_{lower}_channel'

pmem_fmtstr = 'MEMORY_CONTROLLER {name}{{{frequency}, {io_freq}, {tRP}, {tRCD}, {tCAS}, {turn_around_time}, {{{_ulptr}}}}};'
vmem_fmtstr = 'VirtualMemory vmem{{{pte_page_size}, {num_levels}, {minor_fault_penalty}, {dram_name}}};'

queue_fmtstr = 'champsim::channel {name}{{{rq_size}, {pq_size}, {wq_size}, {_offset_bits}, {_queue_check_full_addr:b}}};'

core_builder_parts = {
    'frequency': '.frequency({frequency})',
    'ifetch_buffer_size': '.ifetch_buffer_size({ifetch_buffer_size})',
    'decode_buffer_size': '.decode_buffer_size({decode_buffer_size})',
    'dispatch_buffer_size': '.dispatch_buffer_size({dispatch_buffer_size})',
    'rob_size': '.rob_size({rob_size})',
    'lq_size': '.lq_size({lq_size})',
    'sq_size': '.sq_size({sq_size})',
    'fetch_width': '.fetch_width(champsim::bandwidth::maximum_type{{{fetch_width}}})',
    'decode_width': '.decode_width(champsim::bandwidth::maximum_type{{{decode_width}}})',
    'dispatch_width': '.dispatch_width(champsim::bandwidth::maximum_type{{{dispatch_width}}})',
    'scheduler_size': '.schedule_width(champsim::bandwidth::maximum_type{{{scheduler_size}}})',
    'execute_width': '.execute_width(champsim::bandwidth::maximum_type{{{execute_width}}})',
    'lq_width': '.lq_width(champsim::bandwidth::maximum_type{{{lq_width}}})',
    'sq_width': '.sq_width(champsim::bandwidth::maximum_type{{{sq_width}}})',
    'retire_width': '.retire_width(champsim::bandwidth::maximum_type{{{retire_width}}})',
    'mispredict_penalty': '.mispredict_penalty({mispredict_penalty})',
    'decode_latency': '.decode_latency({decode_latency})',
    'dispatch_latency': '.dispatch_latency({dispatch_latency})',
    'schedule_latency': '.schedule_latency({schedule_latency})',
    'execute_latency': '.execute_latency({execute_latency})',
    'dib_set': '  .dib_set({dib_set})',
    'dib_way': '  .dib_way({dib_way})',
    'dib_window': '  .dib_window({dib_window})',
    'L1I': ['.l1i(&{L1I})', '.l1i_bandwidth({L1I}.MAX_TAG)', '.fetch_queues(&{^fetch_queues})'],
    'L1D': ['.l1d_bandwidth({L1D}.MAX_TAG)', '.data_queues(&{^data_queues})'],
    '_branch_predictor_data': '.branch_predictor<{^branch_predictor_string}>()',
    '_btb_data': '.btb<{^btb_string}>()',
    '_index': '.index({_index})'
}

dib_builder_parts = {
    'sets': '  .dib_set({DIB[sets]})',
    'ways': '  .dib_way({DIB[ways]})',
    'window_size': '  .dib_window({DIB[window_size]})'
}

cache_builder_parts = {
    'name': '.name("{name}")',
    'frequency': '.frequency({frequency})',
    'size': '.size({size})',
    'sets': '.sets({sets})',
    'ways': '.ways({ways})',
    'pq_size': '.pq_size({pq_size})',
    'mshr_size': '.mshr_size({mshr_size})',
    'latency': '.latency({latency})',
    'hit_latency': '.hit_latency({hit_latency})',
    'fill_latency': '.fill_latency({fill_latency})',
    'max_tag_check': '.tag_bandwidth(champsim::bandwidth::maximum_type{{{max_tag_check}}})',
    'max_fill': '.fill_bandwidth(champsim::bandwidth::maximum_type{{{max_fill}}})',
    '_offset_bits': '.offset_bits({_offset_bits})',
    'prefetch_activate': '.prefetch_activate({^prefetch_activate_string})',
    '_replacement_data': '.replacement<{^replacement_string}>()',
    '_prefetcher_data': '.prefetcher<{^prefetcher_string}>()',
    'lower_translate': '.lower_translate(&{^lower_translate_queues})',
    'lower_level': '.lower_level(&{^lower_level_queues})'
}

ptw_builder_parts = {
    'name': '.name("{name}")',
    'cpu': '.cpu({cpu})',
    'lower_level': '.lower_level(&{^lower_level_queues})',
    'mshr_size': '.mshr_size({mshr_size})',
    'max_read': '.tag_bandwidth(champsim::bandwidth::maximum_type{{{max_read}}})',
    'max_write': '.fill_bandwidth(champsim::bandwidth::maximum_type{{{max_write}}})'
}

def vector_string(iterable):
    ''' Produce a string that avoids a warning on clang under -Wbraced-scalar-init if there is only one member '''
    hoisted = list(iterable)
    if len(hoisted) == 1:
        return hoisted[0]
    return '{'+', '.join(hoisted)+'}'

def get_cpu_builder(cpu):
    required_parts = [
    ]

    local_params = {
        '^branch_predictor_string': ', '.join(f'class {k["class"]}' for k in cpu.get('_branch_predictor_data',[])),
        '^btb_string': ', '.join(f'class {k["class"]}' for k in cpu.get('_btb_data',[])),
        '^fetch_queues': channel_name(upper=cpu.get('name'), lower=cpu.get('L1I')),
        '^data_queues': channel_name(upper=cpu.get('name'), lower=cpu.get('L1D'))
    }

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('O3_CPU {name}{{', 'champsim::core_builder{{ champsim::defaults::default_core }}'),
        required_parts,
        *(util.wrap_list(v) for k,v in core_builder_parts.items() if k in cpu),
        (v for k,v in dib_builder_parts.items() if k in cpu.get('DIB',{}))
    ), indent=1, line_end=''), ('}};', ''))
    yield from (part.format(**cpu, **local_params) for part in builder_parts)

def get_cache_builder(elem, upper_levels):
    required_parts = [
        '.upper_levels({{{^upper_levels_string}}})'
    ]

    local_cache_builder_parts = {
        ('prefetch_as_load', True): '.set_prefetch_as_load()',
        ('prefetch_as_load', False): '.reset_prefetch_as_load()',
        ('wq_check_full_addr', True): '.set_wq_checks_full_addr()',
        ('wq_check_full_addr', False): '.reset_wq_checks_full_addr()',
        ('virtual_prefetch', True): '.set_virtual_prefetch()',
        ('virtual_prefetch', False): '.reset_virtual_prefetch()'
    }

    local_params = {
        '^defaults': elem.get('_defaults', ''),
        '^upper_levels_string': vector_string("&"+v for v in upper_levels[elem["name"]]["upper_channels"]),
        '^prefetch_activate_string': ', '.join('access_type::'+t for t in elem.get('prefetch_activate',[])),
        '^replacement_string': ', '.join(f'class {k["class"]}' for k in elem.get('_replacement_data',[])),
        '^prefetcher_string': ', '.join(f'class {k["class"]}' for k in elem.get('_prefetcher_data',[])),
        '^lower_translate_queues': channel_name(upper=elem.get('name'), lower=elem.get('lower_translate')),
        '^lower_level_queues': channel_name(upper=elem.get('name'), lower=elem.get('lower_level'))
    }

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('CACHE {name}{{', 'champsim::cache_builder{{ {^defaults} }}'),
        required_parts,
        (v for k,v in cache_builder_parts.items() if k in elem),
        (v for k,v in local_cache_builder_parts.items() if k[0] in elem and k[1] == elem[k[0]])
    ), indent=1, line_end=''), ('}};', ''))
    yield from (part.format(**elem, **local_params) for part in builder_parts)

def get_ptw_builder(ptw, upper_levels):
    required_parts = [
        '.upper_levels({{{^upper_levels_string}}})',
        '.virtual_memory(&vmem)'
    ]

    local_ptw_builder_parts = {
        ('pscl5_set', 'pscl5_way'): '.add_pscl(5, {pscl5_set}, {pscl5_way})',
        ('pscl4_set', 'pscl4_way'): '.add_pscl(4, {pscl4_set}, {pscl4_way})',
        ('pscl3_set', 'pscl3_way'): '.add_pscl(3, {pscl3_set}, {pscl3_way})',
        ('pscl2_set', 'pscl2_way'): '.add_pscl(2, {pscl2_set}, {pscl2_way})'
    }

    local_params = {
        '^upper_levels_string': vector_string("&"+v for v in upper_levels[ptw["name"]]["upper_channels"]),
        '^lower_level_queues': channel_name(upper=ptw.get('name'), lower=ptw.get('lower_level'))
    }

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('PageTableWalker {name}{{', 'champsim::ptw_builder{{ champsim::defaults::default_ptw }}'),
        required_parts,
        (v for k,v in ptw_builder_parts.items() if k in ptw),
        (v for keys,v in local_ptw_builder_parts.items() if any(k in ptw for k in keys))
    ), indent=1, line_end=''), ('}};', ''))
    yield from (part.format(**ptw, **local_params) for part in builder_parts)

def get_ref_vector_function(rtype, func_name, elements):
    '''
    Generate a C++ function with the given name whose return type is a `std::vector` of `std::reference_wrapper`s to the given type.
    The members of the vector are references to the given elements.
    '''

    if len(elements) > 1:
        open_brace, close_brace = '{{', '}}'
    else:
        open_brace, close_brace = '{', '}'

    wrapped = itertools.chain(
        ('return', open_brace),
        util.append_except_last((f'std::reference_wrapper<{rtype}>{{{elem["name"]}}}' for elem in elements), ','),
        (f'{close_brace};',)
    )
    wrapped = util.multiline(wrapped, length=3, indent=2, line_end='')

    wrapped_rtype = f'std::vector<std::reference_wrapper<{rtype}>>'

    yield from cxx.function(func_name, wrapped, rtype=wrapped_rtype, qualifiers=['override'])
    yield ''

def cache_queue_defaults(cache):
    return {
        'rq_size': cache.get('rq_size', cache['_queue_factor']),
        'wq_size': cache.get('wq_size', cache['_queue_factor']),
        'pq_size': cache.get('pq_size', cache['_queue_factor']),
        '_offset_bits': cache['_offset_bits'],
        '_queue_check_full_addr': cache['_queue_check_full_addr']
    }

def ptw_queue_defaults(ptw):
    return {
        'rq_size': ptw.get('rq_size', ptw['_queue_factor']),
        'wq_size': 0,
        'pq_size': 0,
        '_offset_bits': 'champsim::lg2(PAGE_SIZE)',
        '_queue_check_full_addr': False
    }

def named_selector(elem, key):
    return elem.get(key), elem.get('name')

def upper_channel_collector(grouped_by_lower_level):
    return util.chain(*(
        {lower_name: {'upper_channels': [channel_name(lower=lower_name, upper=upper_name)]}}
        for lower_name, upper_name in grouped_by_lower_level
    ))

def get_upper_levels(cores, caches, ptws):
    return list(filter(lambda x: x[0] is not None, itertools.chain(
        map(functools.partial(named_selector, key='lower_level'), ptws),
        map(functools.partial(named_selector, key='lower_level'), caches),
        map(functools.partial(named_selector, key='lower_translate'), caches),
        map(functools.partial(named_selector, key='L1I'), cores),
        map(functools.partial(named_selector, key='L1D'), cores)
    )))

def check_header_compiles_for_class(clazz, file):
    ''' Check if including the given header file is sufficient to compile an instance of the given class. '''
    champsim_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    include_dir = os.path.join(champsim_root, 'inc')
    vcpkg_parent = os.path.join(champsim_root, 'vcpkg_installed')
    _, triplet_dirs, _ = next(os.walk(vcpkg_parent))
    triplet_dir = os.path.join(vcpkg_parent, next(filter(lambda x: x != 'vcpkg', triplet_dirs), None), 'include')

    with tempfile.TemporaryDirectory() as dtemp:
        args = (
            f'-I{include_dir}',
            f'-I{triplet_dir}',
            f'-I{dtemp}',

            # patch constants
            '-DBLOCK_SIZE=64',
            '-DPAGE_SIZE=4096',
            '-DSTAT_PRINTING_PERIOD=1000000',
            '-DNUM_CPUS=1',
            '-DLOG2_BLOCK_SIZE=6',
            '-DLOG2_PAGE_SIZE=12',

            '-DDRAM_IO_FREQ=3200',
            '-DDRAM_CHANNELS=1',
            '-DDRAM_RANKS=1',
            '-DDRAM_BANKS=1',
            '-DDRAM_ROWS=65536',
            '-DDRAM_COLUMNS=16',
            '-DDRAM_CHANNEL_WIDTH=8',
            '-DDRAM_WQ_SIZE=8',
            '-DDRAM_RQ_SIZE=8'
        )

        # touch this file
        with open(os.path.join(dtemp, 'champsim_constants.h'), 'wt') as wfp:
            print('', file=wfp)

        return cxx.check_compiles((f'#include "{file}"', f'class {clazz} x{{nullptr}};'), *args)

def module_include_files(datas):
    '''
    Generate C++ include lines for all header files necessary to compile the given modules.

    Each module's paths are searched, and compilation checked (linking is not performed. If the compilation succeeds,
    the file is emitted as a candidate.

    A warning is printed if a class is entirely dropped from the list, that is, if it failed to compile with any header.
    In this case, we procede, but ChampSim's compilation will likely fail.
    '''

    def all_headers_on(path):
        for base,_,files in os.walk(path):
            for file in files:
                if os.path.splitext(file)[1] == '.h':
                    yield os.path.abspath(os.path.join(base, file))

    class_paths = (zip(itertools.repeat(module_data['class']), all_headers_on(module_data['path'])) for module_data in datas)
    candidates = list(set(itertools.chain.from_iterable(class_paths)))
    with mp.Pool() as pool:
        successes = pool.starmap(check_header_compiles_for_class, candidates)
    filtered_candidates = list(itertools.compress(candidates, successes))

    class_difference = set(n for n,_ in candidates) - set(n for n,_ in filtered_candidates)
    for clazz in class_difference:
        tried_files = (f for c,f in candidates if c == clazz)
        print('WARNING: no header found for', clazz)
        print('NOTE: after trying files')
        for file in tried_files:
            failed = successes[candidates.index((clazz,file))]
            print('NOTE:', file)
            print('NOTE:', failed.args)
            for line in failed.stderr.splitlines():
                print('NOTE:  ', line)

    yield from (f'#include "{f}"' for _,f in filtered_candidates)

def get_instantiation_lines(cores, caches, ptws, pmem, vmem):
    upper_levels = util.chain(
            *util.collect(get_upper_levels(cores, caches, ptws), operator.itemgetter(0), upper_channel_collector),
            *({c['name']: cache_queue_defaults(c)} for c in caches),
            *({p['name']: ptw_queue_defaults(p)} for p in ptws),
            {pmem['name']: {
                    'rq_size':'std::numeric_limits<std::size_t>::max()',
                    'wq_size':'std::numeric_limits<std::size_t>::max()',
                    'pq_size':'std::numeric_limits<std::size_t>::max()',
                    '_offset_bits':'champsim::lg2(BLOCK_SIZE)',
                    '_queue_check_full_addr':False
                }
            }
        )

    yield '// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): generated magic numbers'
    yield '#include "environment.h"'
    yield '#if __has_include("module_def.inc")'
    yield '#include "module_def.inc"'
    yield '#endif'

    datas = itertools.chain(
        *(c['_branch_predictor_data'] for c in cores),
        *(c['_btb_data'] for c in cores),
        *(c['_prefetcher_data'] for c in caches),
        *(c['_replacement_data'] for c in caches)
    )
    yield from module_include_files(datas)

    yield '#include "defaults.hpp"'
    yield '#include "vmem.h"'
    yield 'namespace champsim::configured {'
    struct_body = itertools.chain(
        *((queue_fmtstr.format(name=ul_queues, **v) for ul_queues in v['upper_channels']) for v in upper_levels.values()),

        (pmem_fmtstr.format(_ulptr=vector_string('&'+v for v in upper_levels[pmem['name']]['upper_channels']), **pmem),),
        (vmem_fmtstr.format(dram_name=pmem['name'], **vmem),),

        *map(functools.partial(get_ptw_builder, upper_levels=upper_levels), ptws),
        *map(functools.partial(get_cache_builder, upper_levels=upper_levels), caches),
        *map(get_cpu_builder, cores),

        get_ref_vector_function('O3_CPU', 'cpu_view', cores),
        get_ref_vector_function('CACHE', 'cache_view', caches),
        get_ref_vector_function('PageTableWalker', 'ptw_view', ptws),
        get_ref_vector_function('champsim::operable', 'operable_view', list(itertools.chain(cores, caches, ptws, (pmem,)))),

        cxx.function('dram_view', [f'return {pmem["name"]};'], rtype='MEMORY_CONTROLLER&', qualifiers=['override'])
    )
    yield from cxx.struct('generated_environment final', struct_body, superclass='champsim::environment')

    yield '}'
    yield '// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)'
