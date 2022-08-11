
def get_pref_data(module_name, is_instruction_cache=False):
    retval = {}

    prefix = 'ipref_' if is_instruction_cache else 'pref_'
    # Resolve prefetcher function names
    retval['prefetcher_initialize'] = prefix + module_name + '_initialize'
    retval['prefetcher_cache_operate'] = prefix + module_name + '_cache_operate'
    retval['prefetcher_branch_operate'] = prefix + module_name + '_branch_operate'
    retval['prefetcher_cache_fill'] = prefix + module_name + '_cache_fill'
    retval['prefetcher_cycle_operate'] = prefix + module_name + '_cycle_operate'
    retval['prefetcher_final_stats'] = prefix + module_name + '_final_stats'

    retval['opts'] = (
    '-Dprefetcher_initialize=' + retval['prefetcher_initialize'],
    '-Dprefetcher_cache_operate=' + retval['prefetcher_cache_operate'],
    '-Dprefetcher_branch_operate=' + retval['prefetcher_branch_operate'],
    '-Dprefetcher_cache_fill=' + retval['prefetcher_cache_fill'],
    '-Dprefetcher_cycle_operate=' + retval['prefetcher_cycle_operate'],
    '-Dprefetcher_final_stats=' + retval['prefetcher_final_stats'],
    )

    return retval

def get_pref_string(pref_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_PREFETCH_MODULES = {len(pref_data)};\n'

    for i,b in enumerate(pref_data):
        retval += f'constexpr static unsigned long long p{b} = 1 << {i};\n'
    retval += '\n'

    retval += '\n'.join('void {prefetcher_initialize}();'.format(**p) for p in pref_data.values())
    retval += '\nvoid impl_prefetcher_initialize()\n{\n    '
    retval += '\n    '.join('if (pref_type[lg2(p{})]) {prefetcher_initialize}();'.format(k,**p) for k,p in pref_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {prefetcher_branch_operate}(uint64_t, uint8_t, uint64_t);\n'.format(**p) for p in pref_data.values() if p['_is_instruction_prefetcher'])
    retval += '\n'.join('void {prefetcher_branch_operate}(uint64_t, uint8_t, uint64_t) {{ assert(false); }}\n'.format(**p) for p in pref_data.values() if not p['_is_instruction_prefetcher'])
    retval += '\nvoid impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)\n{\n    '
    retval += '\n    '.join('if (pref_type[lg2(p{})]) {prefetcher_branch_operate}(ip, branch_type, branch_target);'.format(k,**p) for k,p in pref_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('uint32_t {prefetcher_cache_operate}(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t);'.format(**p) for p in pref_data.values() if not p['prefetcher_cache_operate'].startswith('ooo_cpu'))
    retval += '\nuint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)\n{\n    '
    retval += 'uint32_t result = 0;\n'
    retval += '\n    '.join('if (pref_type[lg2(p{})]) result ^= {prefetcher_cache_operate}(addr, ip, cache_hit, type, metadata_in);\n'.format(k,**p) for k,p in pref_data.items())
    retval += '    return result;'
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('uint32_t {prefetcher_cache_fill}(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);'.format(**p) for p in pref_data.values() if not p['prefetcher_cache_fill'].startswith('ooo_cpu'))
    retval += '\nuint32_t impl_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)\n{\n    '
    retval += 'uint32_t result = 0;\n    '
    retval += '\n    '.join('if (pref_type[lg2(p{})]) result ^= {prefetcher_cache_fill}(addr, set, way, prefetch, evicted_addr, metadata_in);'.format(k,**p) for k,p in pref_data.items())
    retval += '\n    return result;'
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {prefetcher_cycle_operate}();'.format(**p) for p in pref_data.values() if not p['prefetcher_cycle_operate'].startswith('ooo_cpu'))
    retval += '\nvoid impl_prefetcher_cycle_operate()\n{\n    '
    retval += '\n    '.join('if (pref_type[lg2(p{})]) {prefetcher_cycle_operate}();'.format(k,**p) for k,p in pref_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {prefetcher_final_stats}();'.format(**p) for p in pref_data.values() if not p['prefetcher_final_stats'].startswith('ooo_cpu'))
    retval += '\nvoid impl_prefetcher_final_stats()\n{\n    '
    retval += '\n    '.join('if (pref_type[lg2(p{})]) {prefetcher_final_stats}();'.format(k,**p) for k,p in pref_data.items())
    retval += '\n}\n'
    retval += '\n'

    return retval;
