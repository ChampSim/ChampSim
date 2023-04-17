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

from . import util

pmem_fmtstr = 'MEMORY_CONTROLLER {name}{{{frequency}, {io_freq}, {tRP}, {tRCD}, {tCAS}, {turn_around_time}, {{{_ulptr}}}}};'
vmem_fmtstr = 'VirtualMemory vmem{{{pte_page_size}, {num_levels}, {minor_fault_penalty}, {dram_name}}};'

queue_fmtstr = 'champsim::channel {name}{{{rq_size}, {pq_size}, {wq_size}, {_offset_bits}, {_queue_check_full_addr:b}}};'

core_builder_parts = {
    'ifetch_buffer_size': '.ifetch_buffer_size({ifetch_buffer_size})',
    'decode_buffer_size': '.decode_buffer_size({dispatch_buffer_size})',
    'dispatch_buffer_size': '.dispatch_buffer_size({decode_buffer_size})',
    'rob_size': '.rob_size({rob_size})',
    'lq_size': '.lq_size({lq_size})',
    'sq_size': '.sq_size({sq_size})',
    'fetch_width': '.fetch_width({fetch_width})',
    'decode_width': '.decode_width({decode_width})',
    'dispatch_width': '.dispatch_width({dispatch_width})',
    'schedule_size': '.schedule_width({scheduler_size})',
    'execute_width': '.execute_width({execute_width})',
    'lq_width': '.lq_width({lq_width})',
    'sq_width': '.sq_width({sq_width})',
    'retire_width': '.retire_width({retire_width})',
    'mispredict_penalty': '.mispredict_penalty({mispredict_penalty})',
    'decode_latency': '.decode_latency({decode_latency})',
    'dispatch_latency': '.dispatch_latency({dispatch_latency})',
    'schedule_latency': '.schedule_latency({schedule_latency})',
    'execute_latency': '.execute_latency({execute_latency})',
    'dib_set': '  .dib_set({dib_set})',
    'dib_way': '  .dib_way({dib_way})',
    'dib_window': '  .dib_window({dib_window})'
}

dib_builder_parts = {
    'sets': '  .dib_set({DIB[sets]})',
    'ways': '  .dib_way({DIB[ways]})',
    'window_size': '  .dib_window({DIB[window_size]})'
}

cache_builder_parts = {
    'frequency': '.frequency({frequency})',
    'sets': '.sets({sets})',
    'ways': '.ways({ways})',
    'pq_size': '.pq_size({pq_size})',
    'mshr_size': '.mshr_size({mshr_size})',
    'latency': '.latency({latency})',
    'hit_latency': '.hit_latency({hit_latency})',
    'fill_latency': '.fill_latency({fill_latency})',
    'max_tag_check': '.tag_bandwidth({max_tag_check})',
    'max_fill': '.fill_bandwidth({max_fill})',
    '_offset_bits': '.offset_bits({_offset_bits})'
}

default_ptw_queue = {
                'wq_size':0,
                'pq_size':0,
                '_offset_bits':'champsim::lg2(PAGE_SIZE)',
                '_queue_check_full_addr':False
        }

# Avoids a warning on clang under -Wbraced-scalar-init if there is only one member
def vector_string(iterable):
    hoisted = list(iterable)
    if len(hoisted) == 1:
        return hoisted[0]
    return '{'+', '.join(hoisted)+'}'

def get_instantiation_lines(cores, caches, ptws, pmem, vmem):
    upper_level_pairs = tuple(itertools.chain(
        ((elem['lower_level'], elem['name']) for elem in ptws),
        ((elem['lower_level'], elem['name']) for elem in caches),
        ((elem['lower_translate'], elem['name']) for elem in caches if 'lower_translate' in elem),
        *(((elem['L1I'], elem['name']), (elem['L1D'], elem['name'])) for elem in cores)
    ))

    upper_levels = {k: {'uppers': tuple(x[1] for x in v)} for k,v in itertools.groupby(sorted(upper_level_pairs, key=operator.itemgetter(0)), key=operator.itemgetter(0))}

    subdict_keys = ('rq_size', 'pq_size', 'wq_size', '_offset_bits', '_queue_check_full_addr')
    upper_levels = util.chain(upper_levels,
            *({c['name']: util.subdict(c, subdict_keys)} for c in caches),
            *({p['name']: util.chain(default_ptw_queue, util.subdict(p, subdict_keys))} for p in ptws),
            {pmem['name']: {
                    'rq_size':'std::numeric_limits<std::size_t>::max()',
                    'wq_size':'std::numeric_limits<std::size_t>::max()',
                    'pq_size':'std::numeric_limits<std::size_t>::max()',
                    '_offset_bits':'champsim::lg2(BLOCK_SIZE)',
                    '_queue_check_full_addr':False
                }
            }
        )

    yield '#include "environment.h"'
    yield '#include "defaults.hpp"'
    yield '#include "vmem.h"'
    yield 'namespace champsim::configured {'
    yield 'struct generated_environment final : public champsim::environment {'
    yield ''

    for ll,v in upper_levels.items():
        for ul in v['uppers']:
            yield queue_fmtstr.format(name='{}_to_{}_queues'.format(ul, ll), **v)
    yield ''

    yield pmem_fmtstr.format(
            _ulptr=vector_string('&{}_to_{}_queues'.format(ul, pmem['name']) for ul in upper_levels[pmem['name']]['uppers']),
            **pmem)
    yield vmem_fmtstr.format(dram_name=pmem['name'], **vmem)

    for ptw in ptws:
        yield 'PageTableWalker {name}{{PageTableWalker::Builder{{champsim::defaults::default_ptw}}'.format(**ptw)
        yield '.name("{name}")'.format(**ptw)
        yield '.cpu({cpu})'.format(**ptw)
        yield '.virtual_memory(&vmem)'

        if "pscl5_set" in ptw or "pscl5_way" in ptw:
            yield '.add_pscl(5, {pscl5_set}, {pscl5_way})'.format(**ptw)
        if "pscl4_set" in ptw or "pscl4_way" in ptw:
            yield '.add_pscl(4, {pscl4_set}, {pscl4_way})'.format(**ptw)
        if "pscl3_set" in ptw or "pscl3_way" in ptw:
            yield '.add_pscl(3, {pscl3_set}, {pscl3_way})'.format(**ptw)
        if "pscl2_set" in ptw or "pscl2_way" in ptw:
            yield '.add_pscl(2, {pscl2_set}, {pscl2_way})'.format(**ptw)
        if "mshr_size" in ptw:
            yield '.mshr_size({mshr_size})'.format(**ptw)
        if "max_read" in ptw:
            yield '.tag_bandwidth({max_read})'.format(**ptw)
        if "max_write" in ptw:
            yield '.fill_bandwidth({max_write})'.format(**ptw)

        yield '.upper_levels({{{}}})'.format(vector_string('&{}_to_{}_queues'.format(ul, ptw['name']) for ul in upper_levels[ptw['name']]['uppers']))
        yield '.lower_level({})'.format('&{}_to_{}_queues'.format(ptw['name'], ptw['lower_level']))

        yield '};'
        yield ''

    for elem in caches:
        yield 'CACHE {}{{CACHE::Builder{{ {} }}'.format(elem['name'], elem.get('_defaults', ''))
        yield '.name("{name}")'.format(**elem)

        local_cache_builder_parts = {
            ('prefetch_as_load', True): '.set_prefetch_as_load()',
            ('prefetch_as_load', False): '.reset_prefetch_as_load()',
            ('wq_check_full_addr', True): '.set_wq_checks_full_addr()',
            ('wq_check_full_addr', False): '.reset_wq_checks_full_addr()',
            ('virtual_prefetch', True): '.set_virtual_prefetch()',
            ('virtual_prefetch', False): '.reset_virtual_prefetch()'
        }

        yield from (v.format(**elem) for k,v in cache_builder_parts.items() if k in elem)
        yield from (v.format(**elem) for k,v in local_cache_builder_parts.items() if k[0] in elem and k[1] == elem[k[0]])

        # Create prefetch activation masks
        if elem.get('prefetch_activate'):
            type_list = ('LOAD', 'RFO', 'PREFETCH', 'WRITEBACK', 'TRANSLATION')
            yield '.prefetch_activate({})'.format(', '.join(t for t in type_list if t in elem.get('prefetch_activate', tuple())))

        if elem.get('_replacement_data'):
            yield '.replacement<{}>()'.format(' | '.join('CACHE::r{}'.format(k['name']) for k in elem['_replacement_data']))

        if elem.get('_prefetcher_data'):
            yield '.prefetcher<{}>()'.format(' | '.join('CACHE::p{}'.format(k['name']) for k in elem['_prefetcher_data']))

        yield '.upper_levels({{{}}})'.format(vector_string('&{}_to_{}_queues'.format(ul, elem['name']) for ul in upper_levels[elem['name']]['uppers']))
        yield '.lower_level({})'.format('&{}_to_{}_queues'.format(elem['name'], elem['lower_level']))

        if 'lower_translate' in elem:
            yield '.lower_translate({})'.format('&{}_to_{}_queues'.format(elem['name'], elem['lower_translate']))

        yield '};'
        yield ''

    for cpu in cores:
        yield 'O3_CPU {}{{O3_CPU::Builder{{ champsim::defaults::default_core }}'.format(cpu['name'])

        yield '.index({index})'.format(**cpu)
        yield '.frequency({frequency})'.format(**cpu)
        yield '.l1i(&{L1I})'.format(**cpu)
        yield '.l1i_bandwidth({L1I}.MAX_TAG)'.format(**cpu)
        yield '.l1d_bandwidth({L1D}.MAX_TAG)'.format(**cpu)

        yield from (v.format(**cpu) for k,v in core_builder_parts.items() if k in cpu)
        yield from (v.format(**cpu['DIB']) for k,v in dib_builder_parts.items() if k in cpu)

        if cpu.get('_branch_predictor_data'):
            yield '.branch_predictor<{}>()'.format(' | '.join('O3_CPU::b{}'.format(k['name']) for k in cpu['_branch_predictor_data']))
        if cpu.get('_btb_data'):
            yield '.btb<{}>()'.format(' | '.join('O3_CPU::t{}'.format(k['name']) for k in cpu['_btb_data']))

        yield '.fetch_queues({})'.format('&{}_to_{}_queues'.format(cpu['name'], cpu['L1I']))
        yield '.data_queues({})'.format('&{}_to_{}_queues'.format(cpu['name'], cpu['L1D']))

        yield '};'
        yield ''

    yield ''
    yield 'std::vector<std::reference_wrapper<O3_CPU>> cpu_view() override {'
    yield '  return {'
    yield '    ' + ', '.join('std::ref({name})'.format(**elem) for elem in cores)
    yield '  };'
    yield '}'
    yield ''

    yield 'std::vector<std::reference_wrapper<CACHE>> cache_view() override {'
    yield '  return {'
    yield '    ' + ', '.join('{name}'.format(**elem) for elem in caches)
    yield '  };'
    yield '}'
    yield ''

    yield 'std::vector<std::reference_wrapper<PageTableWalker>> ptw_view() override {'
    yield '  return {'
    yield '    ' + ', '.join('{name}'.format(**elem) for elem in ptws)
    yield '  };'
    yield '}'
    yield ''

    yield 'MEMORY_CONTROLLER& dram_view() override {{ return {}; }}'.format(pmem['name'])
    yield ''

    yield 'std::vector<std::reference_wrapper<champsim::operable>> operable_view() override {'
    yield '  return {'
    yield '    ' + ', '.join('{name}'.format(**elem) for elem in itertools.chain(cores, ptws, caches, (pmem,)))
    yield '  };'
    yield '}'
    yield ''

    yield '};'
    yield '}'
