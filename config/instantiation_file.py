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

pmem_fmtstr = 'champsim::chrono::picoseconds{{{clock_period_dbus}}}, champsim::chrono::picoseconds{{{clock_period_mc}}}, std::size_t{{{_tRP}}}, std::size_t{{{_tRCD}}}, std::size_t{{{_tCAS}}}, std::size_t{{{_tRAS}}}, champsim::chrono::microseconds{{{_refresh_period}}}, {{{_ulptr}}}, {rq_size}, {wq_size}, {channels}, champsim::data::bytes{{{channel_width}}}, {_bank_rows}, {_bank_columns}, {ranks}, {bankgroups}, {banks}, {_refreshes_per_period}'
vmem_fmtstr = 'champsim::data::bytes{{{pte_page_size}}}, {num_levels}, champsim::chrono::picoseconds{{{clock_period}*{minor_fault_penalty}}}, {dram_name}, {_randomization}'

queue_fmtstr = '{rq_size}, {pq_size}, {wq_size}, champsim::data::bits{{{_offset_bits}}}, {_queue_check_full_addr:b}'

core_builder_parts = {
    'ifetch_buffer_size': '.ifetch_buffer_size({ifetch_buffer_size})',
    'decode_buffer_size': '.decode_buffer_size({decode_buffer_size})',
    'dispatch_buffer_size': '.dispatch_buffer_size({dispatch_buffer_size})',
    'register_file_size': '.register_file_size({register_file_size})',
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
    'L1I': ['.l1i(&{^l1i_ptr})', '.l1i_bandwidth({^l1i_ptr}.MAX_TAG)', '.fetch_queues(&{^fetch_queues})'],
    'L1D': ['.l1d_bandwidth({^l1d_ptr}.MAX_TAG)', '.data_queues(&{^data_queues})'],
    '_branch_predictor_data': '.branch_predictor<{^branch_predictor_string}>()',
    '_btb_data': '.btb<{^btb_string}>()',
    '_index': '.index({_index})',
    'frequency': '.clock_period(champsim::chrono::picoseconds{{{^clock_period}}})'
}

dib_builder_parts = {
    'sets': '  .dib_set({DIB[sets]})',
    'ways': '  .dib_way({DIB[ways]})',
    'window_size': '  .dib_window({DIB[window_size]})'
}

cache_builder_parts = {
    'size': '.size(champsim::data::bytes{{{size}}})',
    'log2_size': '.log2_size({log2_size})',
    'sets': '.sets({sets})',
    'log2_sets': '.log2_sets({log2_sets})',
    'ways': '.ways({ways})',
    'log2_ways': '.log2_ways({log2_ways})',
    'pq_size': '.pq_size({pq_size})',
    'mshr_size': '.mshr_size({mshr_size})',
    'latency': '.latency({latency})',
    'hit_latency': '.hit_latency({hit_latency})',
    'fill_latency': '.fill_latency({fill_latency})',
    'max_tag_check': '.tag_bandwidth(champsim::bandwidth::maximum_type{{{max_tag_check}}})',
    'max_fill': '.fill_bandwidth(champsim::bandwidth::maximum_type{{{max_fill}}})',
    '_offset_bits': '.offset_bits(champsim::data::bits{{{_offset_bits}}})',
    'prefetch_activate': '.prefetch_activate({^prefetch_activate_string})',
    '_replacement_data': '.replacement<{^replacement_string}>()',
    '_prefetcher_data': '.prefetcher<{^prefetcher_string}>()',
    'lower_translate': '.lower_translate(&{^lower_translate_queues})',
    'lower_level': '.lower_level(&{^lower_level_queues})',
    'frequency': '.clock_period(champsim::chrono::picoseconds{{{^clock_period}}})'
}

ptw_builder_parts = {
    'name': '.name("{name}")',
    'cpu': '.cpu({cpu})',
    'lower_level': '.lower_level(&{^lower_level_queues})',
    'mshr_size': '.mshr_size({mshr_size})',
    'max_read': '.tag_bandwidth(champsim::bandwidth::maximum_type{{{max_read}}})',
    'max_write': '.fill_bandwidth(champsim::bandwidth::maximum_type{{{max_write}}})',
    'frequency': '.clock_period(champsim::chrono::picoseconds{{{^clock_period}}})'
}

def vector_string(iterable):
    ''' Produce a string that avoids a warning on clang under -Wbraced-scalar-init if there is only one member '''
    hoisted = list(iterable)
    if len(hoisted) == 1:
        return hoisted[0]
    return '{'+', '.join(hoisted)+'}'

def get_cpu_builder(cpu, caches, ul_pairs):
    '''
    Generate a champsim::core_builder
    '''
    required_parts = [
    ]

    def cache_index(name):
        return next(filter(lambda x: x[1]['name'] == name, enumerate(caches)))[0]

    local_params = {
        '^branch_predictor_string': ', '.join(f'class {k["class"]}' for k in cpu.get('_branch_predictor_data',[])),
        '^btb_string': ', '.join(f'class {k["class"]}' for k in cpu.get('_btb_data',[])),
        '^fetch_queues': f'channels.at({ul_pairs.index((cpu.get("L1I"), cpu.get("name")))})',
        '^data_queues': f'channels.at({ul_pairs.index((cpu.get("L1D"), cpu.get("name")))})',
        '^l1i_ptr': f'(*std::next(std::begin(caches), {cache_index(cpu.get("L1I"))}))',
        '^l1d_ptr': f'(*std::next(std::begin(caches), {cache_index(cpu.get("L1D"))}))'
    }
    if 'frequency' in cpu:
        local_params['^clock_period'] = int(1000000/cpu['frequency'])

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('champsim::core_builder{{ champsim::defaults::default_core }}',),
        required_parts,
        *(util.wrap_list(v) for k,v in core_builder_parts.items() if k in cpu),
        (v for k,v in dib_builder_parts.items() if k in cpu.get('DIB',{}))
    ), indent=1, line_end=''))
    yield from (part.format(**cpu, **local_params) for part in builder_parts)

def get_cache_builder(elem, ul_pairs):
    '''
    Generate a champsim::cache_builder
    '''
    required_parts = [
        '.name("{name}")',
        '.upper_levels({{{^upper_levels_string}}})',
    ]

    local_cache_builder_parts = {
        ('prefetch_as_load', True): '.set_prefetch_as_load()',
        ('prefetch_as_load', False): '.reset_prefetch_as_load()',
        ('wq_check_full_addr', True): '.set_wq_checks_full_addr()',
        ('wq_check_full_addr', False): '.reset_wq_checks_full_addr()',
        ('virtual_prefetch', True): '.set_virtual_prefetch()',
        ('virtual_prefetch', False): '.reset_virtual_prefetch()'
    }

    uppers = (v for v in ul_pairs if v[0] == elem.get('name'))
    local_params = {
        '^defaults': elem.get('_defaults', ''),
        '^upper_levels_string': vector_string(f'&channels.at({ul_pairs.index(v)})' for v in uppers),
        '^prefetch_activate_string': ', '.join('access_type::'+t for t in elem.get('prefetch_activate',[])),
        '^replacement_string': ', '.join(f'class {k["class"]}' for k in elem.get('_replacement_data',[])),
        '^prefetcher_string': ', '.join(f'class {k["class"]}' for k in elem.get('_prefetcher_data',[])),
        '^lower_level_queues': f'channels.at({ul_pairs.index((elem.get("lower_level"), elem.get("name")))})'
    }
    if 'frequency' in elem:
        local_params['^clock_period'] = int(1000000/elem['frequency'])
    if 'lower_translate' in elem:
        local_params.update({
            '^lower_translate_queues': f'channels.at({ul_pairs.index((elem.get("lower_translate"), elem.get("name")))})'
        })

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('champsim::cache_builder{{ {^defaults} }}',),
        required_parts,
        (v for k,v in cache_builder_parts.items() if k in elem),
        (v for k,v in local_cache_builder_parts.items() if k[0] in elem and k[1] == elem[k[0]])
    ), indent=1, line_end=''))
    yield from (part.format(**elem, **local_params) for part in builder_parts)

def get_ptw_builder(ptw, ul_pairs):
    '''
    Generate a champsim::ptw_builder
    '''
    required_parts = [
        '.name("{name}")',
        '.upper_levels({{{^upper_levels_string}}})',
        '.virtual_memory(&vmem)'
    ]

    local_ptw_builder_parts = {
        ('pscl5_set', 'pscl5_way'): '.add_pscl(5, {pscl5_set}, {pscl5_way})',
        ('pscl4_set', 'pscl4_way'): '.add_pscl(4, {pscl4_set}, {pscl4_way})',
        ('pscl3_set', 'pscl3_way'): '.add_pscl(3, {pscl3_set}, {pscl3_way})',
        ('pscl2_set', 'pscl2_way'): '.add_pscl(2, {pscl2_set}, {pscl2_way})'
    }

    uppers = (v for v in ul_pairs if v[0] == ptw.get('name'))
    local_params = {
        '^upper_levels_string': vector_string(f'&channels.at({ul_pairs.index(v)})' for v in uppers),
        '^lower_level_queues': f'channels.at({ul_pairs.index((ptw.get("lower_level"), ptw.get("name")))})'
    }
    if 'frequency' in ptw:
        local_params['^clock_period'] = int(1000000/ptw['frequency'])

    builder_parts = itertools.chain(util.multiline(itertools.chain(
        ('champsim::ptw_builder{{ champsim::defaults::default_ptw }}',),
        required_parts,
        (v for k,v in ptw_builder_parts.items() if k in ptw),
        (v for keys,v in local_ptw_builder_parts.items() if any(k in ptw for k in keys))
    ), indent=1, line_end=''))
    yield from (part.format(**ptw, **local_params) for part in builder_parts)

def get_ref_vector_function(rtype, func_name, basename):
    '''
    Generate a C++ function with the given name whose return type is a
    `std::vector` of `std::reference_wrapper`s to the given type.
    The members of the vector are references to the given elements.
    '''
    wrapped_rtype = f'std::vector<std::reference_wrapper<{rtype}>>'
    wrapped = (
        f'{wrapped_rtype} retval{{}};',
        'auto make_ref = [](auto& x){ return std::ref(x); };',
        f'std::transform(std::begin({basename}), std::end({basename}), std::back_inserter(retval), make_ref);',
        'return retval;'
    )

    yield from cxx.function(func_name, wrapped, rtype=wrapped_rtype)
    yield ''

def get_builder_function_call(class_name, builders):
    '''
    Generate a call to a function that consumes builders.

    :param class_name: The name of the C++ class to build.
    :param builders: A sequence of builders to pass as parameters.
    '''
    yield f'build<{class_name}>('

    builder_head, builder_tail = util.cut(builders, n=-1)
    for b in builder_head:
        head, tail = util.cut(b, n=-1)
        yield from ('  '+l for l in head)
        yield from ('  '+l+',' for l in tail)

    for b in builder_tail:
        yield from ('  '+l for l in b)

    yield ')'

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

def get_upper_levels(cores, caches, ptws):
    ''' Get a sequence of (lower_name, upper_name) for the given elements. '''
    def named_selector(elem, key):
        return elem.get(key), elem.get('name')

    return list(filter(lambda x: x[0] is not None, itertools.chain(
        map(functools.partial(named_selector, key='lower_level'), ptws),
        map(functools.partial(named_selector, key='lower_level'), caches),
        map(functools.partial(named_selector, key='lower_translate'), caches),
        map(functools.partial(named_selector, key='L1I'), cores),
        map(functools.partial(named_selector, key='L1D'), cores)
    )))

def module_include_files(datas):
    '''
    Generate C++ include lines for all header files necessary to compile the given modules.

    It is assumed that all header files in the directory contribute to compilation.
    '''

    def all_headers_on(path):
        for base,_,files in os.walk(path):
            for file in files:
                if os.path.splitext(file)[1] == '.h':
                    yield os.path.abspath(os.path.join(base, file))

    class_paths = (zip(itertools.repeat(module_data['class']), all_headers_on(module_data['path'])) for module_data in datas)
    candidates = set(itertools.chain.from_iterable(class_paths))

    yield from (f'#include "{f}"' for _,f in candidates)

def decorate_queues(caches, ptws, pmem):
    return util.chain(
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

def get_queue_info(ul_pairs, decoration):
    return [decoration.get(ll) for ll,_ in ul_pairs]

def get_instantiation_lines(cores, caches, ptws, pmem, vmem, build_id):
    '''
    Generate the lines for a C++ file that instantiates a configuration.
    '''
    classname = f'champsim::configured::generated_environment<0x{build_id}>'
    ul_pairs = get_upper_levels(cores, caches, ptws)
    queues = get_queue_info(ul_pairs, decorate_queues(caches, ptws, pmem))

    datas = itertools.filterfalse(operator.methodcaller('get', 'legacy', False), itertools.chain(
        *(c['_branch_predictor_data'] for c in cores),
        *(c['_btb_data'] for c in cores),
        *(c['_prefetcher_data'] for c in caches),
        *(c['_replacement_data'] for c in caches)
    ))
    yield from module_include_files(datas)

    # Get fastest clock period in picoseconds
    global_clock_period = int(1000000/max(x['frequency'] for x in itertools.chain(cores, caches, ptws, (pmem,))))

    channels_head, channels_tail = util.cut((f'champsim::channel{{{queue_fmtstr.format(**v)}}}' for v in queues), n=-1)
    channel_instantiation_body = ('channels{', *(v+',' for v in channels_head), *channels_tail, '},')

    pmem_instantiation_body = (
        'DRAM{',
        pmem_fmtstr.format(
            clock_period_dbus=int(1000000/pmem['data_rate']),
            clock_period_mc=int(1000000/pmem['frequency']),
            _tRP=int(pmem['tRP']),
            _tRCD=int(pmem['tRCD']),
            _tCAS=int(pmem['tCAS']),
            _tRAS=int(pmem['tRAS']),
            _bank_rows=int(pmem['bank_rows']), #added for supporting old configs, mainly column size change
            _bank_columns=int(pmem['columns']*8 if 'columns' in pmem else pmem['bank_columns']),
            _refresh_period=int(1000*pmem['refresh_period']),
            _refreshes_per_period=int(pmem['refreshes_per_period']),
            _ulptr=vector_string(f'&channels.at({ul_pairs.index(v)})' for v in ul_pairs if v[0] == pmem['name']),
            **pmem),
        '},'
    )

    vmem_instantiation_body = (
        'vmem{',
        vmem_fmtstr.format(
            dram_name=pmem['name'], 
            clock_period=global_clock_period,
            _randomization= '{}' if (isinstance(vmem['randomization'],bool) and vmem['randomization'] == False) else int(vmem['randomization']),
            **vmem),
        '},',
    )

    ptw_instantiation_body = (
        'ptws {',
        *get_builder_function_call('PageTableWalker', map(functools.partial(get_ptw_builder, ul_pairs=ul_pairs), ptws)),
        '},'
    )

    cache_instantiation_body = (
        'caches {',
        *get_builder_function_call('CACHE', map(functools.partial(get_cache_builder, ul_pairs=ul_pairs), caches)),
        '},'
    )

    core_instantiation_body = (
        'cores {',
        *get_builder_function_call('O3_CPU',
                                   map(functools.partial(get_cpu_builder, caches=caches, ul_pairs=ul_pairs), cores)),
        '}'
    )

    yield f'champsim::configured::generated_environment<0x{build_id}>::generated_environment() :'
    yield from itertools.chain(
    )
    yield from channel_instantiation_body
    yield from pmem_instantiation_body
    yield from vmem_instantiation_body
    yield from ptw_instantiation_body
    yield from cache_instantiation_body
    yield from core_instantiation_body
    yield '{'
    yield '}'
    yield ''

    yield from get_ref_vector_function('O3_CPU', f'{classname}::cpu_view', 'cores')
    yield ''

    yield from get_ref_vector_function('CACHE', f'{classname}::cache_view', 'caches')
    yield ''

    yield from get_ref_vector_function('PageTableWalker', f'{classname}::ptw_view', 'ptws')
    yield ''

    yield from cxx.function(f'{classname}::operable_view', (
        'std::vector<std::reference_wrapper<champsim::operable>> retval{};',
        'auto make_ref = [](auto& x){ return std::ref<champsim::operable>(x); };',
        'std::transform(std::begin(cores), std::end(cores), std::back_inserter(retval), make_ref);',
        'std::transform(std::begin(caches), std::end(caches), std::back_inserter(retval), make_ref);',
        'std::transform(std::begin(ptws), std::end(ptws), std::back_inserter(retval), make_ref);',
        'retval.push_back(std::ref<champsim::operable>(DRAM));',
        'return retval;'
    ), rtype='std::vector<std::reference_wrapper<champsim::operable>>')
    yield ''

    yield from cxx.function(f'{classname}::dram_view', [f'return {pmem["name"]};'], rtype='MEMORY_CONTROLLER&')
    yield ''

def get_instantiation_header(num_cpus, env, build_id):
    yield '#include "environment.h"'
    yield '#include "vmem.h"'
    yield '#include <forward_list>'
    yield 'template <>'
    struct_body = (
        'private:',
        'std::vector<champsim::channel> channels;',
        'MEMORY_CONTROLLER DRAM;',
        'VirtualMemory vmem;',
        'std::forward_list<PageTableWalker> ptws;',
        'std::forward_list<CACHE> caches;',
        'std::forward_list<O3_CPU> cores;',

        'public:',
        f'constexpr static std::size_t num_cpus = {num_cpus};',
        f'constexpr static std::size_t block_size = {env["block_size"]};',
        f'constexpr static std::size_t page_size = {env["page_size"]};',

        'generated_environment();',
        'std::vector<std::reference_wrapper<O3_CPU>> cpu_view() final;',
        'std::vector<std::reference_wrapper<CACHE>> cache_view() final;',
        'std::vector<std::reference_wrapper<PageTableWalker>> ptw_view() final;',
        'MEMORY_CONTROLLER& dram_view() final;',
        'std::vector<std::reference_wrapper<operable>> operable_view() final;'
    )
    struct_name = f'champsim::configured::generated_environment<0x{build_id}> final'
    yield from cxx.struct(struct_name, struct_body, superclass='champsim::environment')
