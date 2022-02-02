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
import operator

from . import util

ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {frequency}, {{{{{pscl5_set}, {pscl5_way}}}, {{{pscl4_set}, {pscl4_way}}}, {{{pscl3_set}, {pscl3_way}}}, {{{pscl2_set}, {pscl2_way}}}}}, {rq_size}, {mshr_size}, {max_read}, {max_write}, 1, {{{{{_ulptr}}}}}, {_llptr}, vmem);'

cpu_fmtstr = '{{{index}, {frequency}, {{{DIB[sets]}, {DIB[ways]}, {{champsim::lg2({DIB[window_size]})}}, {{champsim::lg2({DIB[window_size]})}}}}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{L1I}, {_l1iptr}, {L1I}.MAX_TAG, {_l1dptr}, {L1D}.MAX_TAG, {branch_enum_string}, {btb_enum_string}}}'

pmem_fmtstr = 'MEMORY_CONTROLLER {name}{{{frequency}, {io_freq}, {tRP}, {tRCD}, {tCAS}, {turn_around_time}, {{{{{_ulptr}}}}}}};'
vmem_fmtstr = 'VirtualMemory vmem({pte_page_size}, {num_levels}, {minor_fault_penalty}, {dram_name});'

cache_fmtstr = 'CACHE {name}{{"{name}", {frequency}, {sets}, {ways}, {mshr_size}, {pq_size}, {hit_latency}, {fill_latency}, {max_tag_check}, {max_fill}, {_offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, {prefetch_activate_mask}, {{{{{_ulptr}}}}}, {_ltptr}, {_llptr}, {pref_enum_string}, {repl_enum_string}}};'
queue_fmtstr = 'champsim::channel {name}{{{rq_size}, {pq_size}, {wq_size}, {_offset_bits}, {wq_check_full_addr:b}}};'

default_ptw_queue = {
                'wq_size':0,
                'pq_size':0,
                '_offset_bits':'champsim::lg2(PAGE_SIZE)',
                'wq_check_full_addr':False
        }

def get_instantiation_lines(cores, caches, ptws, pmem, vmem):
    upper_level_pairs = tuple(itertools.chain(
        ((elem['lower_level'], elem['name']) for elem in ptws),
        ((elem['lower_level'], elem['name']) for elem in caches),
        ((elem['lower_translate'], elem['name']) for elem in caches if 'lower_translate' in elem),
        *(((elem['L1I'], elem['name']), (elem['L1D'], elem['name'])) for elem in cores)
    ))

    upper_levels = {k: {'uppers': tuple(x[1] for x in v)} for k,v in itertools.groupby(sorted(upper_level_pairs, key=operator.itemgetter(0)), key=operator.itemgetter(0))}

    subdict_keys = ('rq_size', 'pq_size', 'wq_size', '_offset_bits', 'wq_check_full_addr')
    upper_levels = util.chain(upper_levels,
            *({c['name']: util.subdict(c, subdict_keys)} for c in caches),
            *({p['name']: util.chain(default_ptw_queue, util.subdict(p, subdict_keys))} for p in ptws),
            {pmem['name']: {
                    'rq_size':'std::numeric_limits<std::size_t>::max()',
                    'wq_size':'std::numeric_limits<std::size_t>::max()',
                    'pq_size':'std::numeric_limits<std::size_t>::max()',
                    '_offset_bits':'champsim::lg2(BLOCK_SIZE)',
                    'wq_check_full_addr':False
                }
            }
        )

    for ll,v in upper_levels.items():
        for ul in v['uppers']:
            yield queue_fmtstr.format(name='{}_to_{}_queues'.format(ul, ll), **v)
    yield ''

    yield pmem_fmtstr.format(
            _ulptr=', '.join('&{}_to_{}_queues'.format(ul, pmem['name']) for ul in upper_levels[pmem['name']]['uppers']),
            **pmem)
    yield vmem_fmtstr.format(dram_name=pmem['name'], **vmem)

    for elem in ptws:
        yield ptw_fmtstr.format(
            _ulptr=', '.join('&{}_to_{}_queues'.format(ul, elem['name']) for ul in upper_levels[elem['name']]['uppers']),
            _llptr='&{}_to_{}_queues'.format(elem['name'], elem['lower_level']),
            **elem)

    for elem in caches:
        yield 'CACHE {}{{CACHE::Builder{{ {} }}'.format(elem['name'], elem.get('_defaults', ''))
        yield '.name("{name}")'.format(**elem)

        cache_builder_parts = {
            'frequency': '.frequency({frequency})',
            'sets': '.sets({sets})',
            'ways': '.ways({ways})',
            'pq_size': '.pq_size({pq_size})',
            'mshr_size': '.mshr_size({mshr_size})',
            'latency': '.latency({latency})',
            'hit_latency': '.hit_latency({hit_latency})',
            'fill_latency': '.fill_latency({fill_latency})',
            'max_read': '.max_read({max_read})',
            'max_write': '.max_write({max_write})',
            'offset_bits': '.offset_bits({offset_bits})',
            'prefetch_as_load': '.set_prefetch_as_load(' + ('true' if elem.get('prefetch_as_load') else 'false') + ')',
            'wq_check_full_addr': '.set_wq_checks_full_addr(' + ('true' if elem.get('wq_check_full_addr') else 'false') + ')',
            'virtual_prefetch': '.set_virtual_prefetch(' + ('true' if elem.get('virtual_prefetch') else 'false') + ')'
        }

        yield from (v.format(**elem) for k,v in cache_builder_parts.items() if k in elem)

        # Create prefetch activation masks
        type_list = ('LOAD', 'RFO', 'PREFETCH', 'WRITEBACK', 'TRANSLATION')
        yield '.prefetch_activate({})'.format(' | '.join('(1 << {})'.format(t) for t in type_list if t in elem.get('prefetch_activate', tuple())))

        yield '.replacement({})'.format(' | '.join(f'CACHE::r{k}' for k in elem['_replacement_modnames']))
        yield '.prefetcher({})'.format(' | '.join(f'CACHE::p{k}' for k in elem['_prefetcher_modnames']))
        yield '.upper_levels({{{}}})'.format(', '.join('&{}_to_{}_queues'.format(ul, elem['name']) for ul in upper_levels[elem['name']]['uppers']))
        yield '.lower_level({})'.format('&{}_to_{}_queues'.format(elem['name'], elem['lower_level']))

        if 'lower_translate' in elem:
            yield '.lower_translate({})'.format('&{}_to_{}_queues'.format(elem['name'], elem['lower_translate']))

        yield '};'
        yield ''

    yield from ('O3_CPU ' + cpu['name'] + cpu_fmtstr.format(
                _l1iptr='&{}_to_{}_queues'.format(cpu['name'], cpu['L1I']),
                _l1dptr='&{}_to_{}_queues'.format(cpu['name'], cpu['L1D']),
                branch_enum_string=' | '.join(f'O3_CPU::b{k}' for k in cpu['_branch_predictor_modnames']),
                btb_enum_string=' | '.join(f'O3_CPU::t{k}' for k in cpu['_btb_modnames']),
                **cpu) + ';' for cpu in cores)
    yield ''

    yield 'std::vector<std::reference_wrapper<O3_CPU>> ooo_cpu {{'
    yield ', '.join('{name}'.format(**elem) for elem in cores)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<CACHE>> caches {{'
    yield ', '.join('{name}'.format(**elem) for elem in caches)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<PageTableWalker>> ptws {{'
    yield ', '.join('{name}'.format(**elem) for elem in ptws)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<champsim::operable>> operables {{'
    yield ', '.join('{name}'.format(**elem) for elem in itertools.chain(cores, ptws, caches, (pmem,)))
    yield '}};'
    yield ''

