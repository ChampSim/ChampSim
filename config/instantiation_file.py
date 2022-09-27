import itertools
import operator

from . import util

ptw_fmtstr = 'PageTableWalker {name}("{name}", {cpu}, {frequency}, {{{{{pscl5_set}, {pscl5_way}, vmem.shamt(4)}}, {{{pscl4_set}, {pscl4_way}, vmem.shamt(3)}}, {{{pscl3_set}, {pscl3_way}, vmem.shamt(2)}}, {{{pscl2_set}, {pscl2_way}, vmem.shamt(1)}}}}, {ptw_rq_size}, {ptw_mshr_size}, {ptw_max_read}, {ptw_max_write}, 1, &{lower_level}, vmem);\n'

cpu_fmtstr = '{{{index}, {frequency}, {{{DIB[sets]}, {DIB[ways]}, {DIB[window_size]}}}, {ifetch_buffer_size}, {dispatch_buffer_size}, {decode_buffer_size}, {rob_size}, {lq_size}, {sq_size}, {fetch_width}, {decode_width}, {dispatch_width}, {scheduler_size}, {execute_width}, {lq_width}, {sq_width}, {retire_width}, {mispredict_penalty}, {decode_latency}, {dispatch_latency}, {schedule_latency}, {execute_latency}, &{L1I}, {L1I}.MAX_READ, &{L1D}, {L1D}.MAX_READ, {branch_enum_string}, {btb_enum_string}}}'

pmem_fmtstr = 'MEMORY_CONTROLLER {name}({frequency}, {io_freq}, {tRP}, {tRCD}, {tCAS}, {turn_around_time});\n'
vmem_fmtstr = 'VirtualMemory vmem(lg2({size}), 1 << 12, {num_levels}, {minor_fault_penalty}, {dram_name});\n'


cache_fmtstr = 'CACHE {name}{{"{name}", {frequency}, {sets}, {ways}, {mshr_size}, {fill_latency}, {max_read}, {max_write}, {_offset_bits}, {prefetch_as_load:b}, {wq_check_full_addr:b}, {virtual_prefetch:b}, {prefetch_activate_mask}, {name}_queues, &{lower_level}, {pref_enum_string}, {repl_enum_string}}};\n'
queue_fmtstr = 'CACHE::{_type} {name}_queues{{{frequency}, {rq_size}, {pq_size}, {wq_size}, {hit_latency}, {_offset_bits}, {wq_check_full_addr:b}}};\n'

file_header = '''
    #include "cache.h"
    #include "champsim.h"
    #include "dram_controller.h"
    #include "ooo_cpu.h"
    #include "ptw.h"
    #include "vmem.h"
    #include "operable.h"
    #include "util.h"
    #include <array>
    #include <functional>
    #include <vector>
    '''

def get_instantiation_string(cores, caches, ptws, pmem, vmem):
    memory_system = {c['name']:c for c in itertools.chain(caches, ptws)}

    # Give each element a fill level
    fill_levels = itertools.chain(*(enumerate(c['name'] for c in util.iter_system(memory_system, cpu[name])) for cpu,name in itertools.product(cores, ('ITLB', 'DTLB', 'L1I', 'L1D'))))
    fill_levels = sorted(fill_levels, key=operator.itemgetter(1))
    fill_levels = ({'name': n, '_fill_level': max(l[0] for l in fl)} for n,fl in itertools.groupby(fill_levels, key=operator.itemgetter(1)))
    memory_system = util.combine_named(fill_levels, memory_system.values())

    # Remove name index
    memory_system = sorted(memory_system.values(), key=operator.itemgetter('_fill_level'), reverse=True)

    instantiation_file = file_header
    instantiation_file += pmem_fmtstr.format(**pmem)
    instantiation_file += '\n'
    instantiation_file += vmem_fmtstr.format(dram_name=pmem['name'], **vmem)
    instantiation_file += '\n'

    for elem in memory_system:
        if 'pscl5_set' in elem:
            instantiation_file += ptw_fmtstr.format(**elem)
        else:
            instantiation_file += queue_fmtstr.format(
                _type = 'TranslatingQueues' if elem.get('_needs_translate') else 'NonTranslatingQueues',
                **elem)
            instantiation_file += cache_fmtstr.format(
                prefetch_activate_mask=' | '.join(f'(1 << {t})' for t in elem['prefetch_activate'].split(',')),
                repl_enum_string=' | '.join(f'CACHE::r{k}' for k in elem['_replacement_modnames']),\
                pref_enum_string=' | '.join(f'CACHE::p{k}' for k in elem['_prefetcher_modnames']),\
                **elem)


    instantiation_file += ''.join(
            'O3_CPU ' + cpu['name'] + cpu_fmtstr.format(
                branch_enum_string=' | '.join(f'O3_CPU::b{k}' for k in cpu['_branch_predictor_modnames']),
                btb_enum_string=' | '.join(f'O3_CPU::t{k}' for k in cpu['_btb_modnames']),
                **cpu) + ';\n' for cpu in cores)

    instantiation_file += 'std::vector<std::reference_wrapper<O3_CPU>> ooo_cpu {{\n'
    instantiation_file += ', '.join('{name}'.format(**elem) for elem in cores)
    instantiation_file += '\n}};\n'

    instantiation_file += 'std::vector<std::reference_wrapper<CACHE>> caches {{\n'
    instantiation_file += ', '.join('{name}'.format(**elem) for elem in reversed(memory_system) if 'pscl5_set' not in elem)
    instantiation_file += '\n}};\n'

    instantiation_file += 'std::vector<std::reference_wrapper<PageTableWalker>> ptws {{\n'
    instantiation_file += ', '.join('{name}'.format(**elem) for elem in reversed(memory_system) if 'pscl5_set' in elem)
    instantiation_file += '\n}};\n'

    instantiation_file += 'std::vector<std::reference_wrapper<champsim::operable>> operables {{\n'
    instantiation_file += ', '.join('{name}'.format(**elem) for elem in cores)
    instantiation_file += ',\n'
    instantiation_file += ', '.join('{name}'.format(**elem) for elem in memory_system if 'pscl5_set' in elem)
    instantiation_file += ',\n'
    instantiation_file += ', '.join('{name}, {name}_queues'.format(**elem) for elem in memory_system if 'pscl5_set' not in elem)
    instantiation_file += ',\n'
    instantiation_file += '{name}'.format(**pmem)
    instantiation_file += '\n}};\n'

    instantiation_file += '\nvoid init_structures() {\n'
    for elem in memory_system:
        if elem.get('_needs_translate'):
            instantiation_file += '  {name}_queues.lower_level = &{lower_translate};\n'.format(**elem)
    instantiation_file += '}\n'

    return instantiation_file

