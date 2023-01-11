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

ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {frequency}, {{{{{pscl5_set}, {pscl5_way}}}, {{{pscl4_set}, {pscl4_way}}}, {{{pscl3_set}, {pscl3_way}}}, {{{pscl2_set}, {pscl2_way}}}}}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, 1, &{lower_level}, vmem);'

cpu_fmtstr = '{{{index}, {frequency}, {{{DIB[sets]}, {DIB[ways]}, {{champsim::lg2({DIB[window_size]})}}, {{champsim::lg2({DIB[window_size]})}}}}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{L1I}, {L1I}.MAX_TAG, &{L1D}, {L1D}.MAX_TAG, {branch_enum_string}, {btb_enum_string}}}'

pmem_fmtstr = 'MEMORY_CONTROLLER {name}({frequency}, {io_freq}, {tRP}, {tRCD}, {tCAS}, {turn_around_time});'
vmem_fmtstr = 'VirtualMemory vmem({pte_page_size}, {num_levels}, {minor_fault_penalty}, {dram_name});'

cache_fmtstr = 'CACHE {name}{{"{name}", {frequency}, {sets}, {ways}, {mshr_size}, {fill_latency}, {max_tag_check}, {max_fill}, {_offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, {prefetch_activate_mask}, {name}_queues, &{lower_level}, {pref_enum_string}, {repl_enum_string}}};'
queue_fmtstr = 'CACHE::{_type} {name}_queues{{{frequency}, {rq_size}, {pq_size}, {wq_size}, {ptwq_size}, {hit_latency}, {_offset_bits}, {wq_check_full_addr:b}}};'

def get_instantiation_lines(cores, caches, ptws, pmem, vmem):
    memory_system = {c['name']:c for c in itertools.chain(caches, ptws)}

    # Give each element a fill level
    fill_levels = itertools.chain(*(enumerate(c['name'] for c in util.iter_system(memory_system, cpu[name])) for cpu,name in itertools.product(cores, ('ITLB', 'DTLB', 'L1I', 'L1D'))))
    fill_levels = sorted(fill_levels, key=operator.itemgetter(1))
    fill_levels = ({'name': n, '_fill_level': max(l[0] for l in fl)} for n,fl in itertools.groupby(fill_levels, key=operator.itemgetter(1)))
    memory_system = util.combine_named(fill_levels, memory_system.values())

    # Remove name index
    memory_system = sorted(memory_system.values(), key=operator.itemgetter('_fill_level'), reverse=True)

    yield pmem_fmtstr.format(**pmem)
    yield vmem_fmtstr.format(dram_name=pmem['name'], **vmem)

    for elem in memory_system:
        if 'pscl5_set' in elem:
            yield ptw_fmtstr.format(**elem)
        else:
            yield queue_fmtstr.format(
                _type = 'TranslatingQueues' if elem.get('_needs_translate') else 'NonTranslatingQueues',
                **elem)
            yield cache_fmtstr.format(
                prefetch_activate_mask=' | '.join(f'(1 << {t})' for t in elem['prefetch_activate'].split(',')),
                repl_enum_string=' | '.join(f'CACHE::r{k}' for k in elem['_replacement_modnames']),\
                pref_enum_string=' | '.join(f'CACHE::p{k}' for k in elem['_prefetcher_modnames']),\
                **elem)


    yield from ('O3_CPU ' + cpu['name'] + cpu_fmtstr.format(
                branch_enum_string=' | '.join(f'O3_CPU::b{k}' for k in cpu['_branch_predictor_modnames']),
                btb_enum_string=' | '.join(f'O3_CPU::t{k}' for k in cpu['_btb_modnames']),
                **cpu) + ';' for cpu in cores)

    yield 'std::vector<std::reference_wrapper<O3_CPU>> ooo_cpu {{'
    yield ', '.join('{name}'.format(**elem) for elem in cores)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<CACHE>> caches {{'
    yield ', '.join('{name}'.format(**elem) for elem in reversed(memory_system) if 'pscl5_set' not in elem)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<PageTableWalker>> ptws {{'
    yield ', '.join('{name}'.format(**elem) for elem in reversed(memory_system) if 'pscl5_set' in elem)
    yield '}};'

    yield 'std::vector<std::reference_wrapper<champsim::operable>> operables {{'
    yield ', '.join('{name}'.format(**elem) for elem in cores) + ','
    yield ', '.join('{name}'.format(**elem) for elem in memory_system if 'pscl5_set' in elem) + ','
    yield ', '.join('{name}, {name}_queues'.format(**elem) for elem in memory_system if 'pscl5_set' not in elem) + ','
    yield '{name}'.format(**pmem)
    yield '}};'
    yield ''

    yield 'void init_structures() {'
    yield from ('  {name}_queues.lower_level = &{lower_translate};'.format(**elem) for elem in memory_system if elem.get('_needs_translate'))
    yield '}'

