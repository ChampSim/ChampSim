
def get_repl_data(module_name):
    retval = {}

    # Resolve cache replacment function names
    retval['init_func_name'] = 'repl_' + module_name + '_initialize'
    retval['find_victim_func_name'] = 'repl_' + module_name + '_victim'
    retval['update_func_name'] = 'repl_' + module_name + '_update'
    retval['final_func_name'] = 'repl_' + module_name + '_final_stats'

    retval['opts'] = (
    '-Dinitialize_replacement=' + retval['init_func_name'],
    '-Dfind_victim=' + retval['find_victim_func_name'],
    '-Dupdate_replacement_state=' + retval['update_func_name'],
    '-Dreplacement_final_stats=' + retval['final_func_name']
    )

    return retval

def get_repl_string(repl_data):
    retval = ''
    retval += f'constexpr static std::size_t NUM_REPLACEMENT_MODULES = {len(repl_data)};\n'

    for i,b in enumerate(repl_data):
        retval += f'constexpr static unsigned long long r{b} = 1 << {i};\n'
    retval += '\n'

    retval += '\n'.join('void {init_func_name}();'.format(**r) for r in repl_data.values())
    retval += '\nvoid impl_replacement_initialize()\n{\n    '
    retval += '\n    '.join('if (repl_type[lg2(r{})]) {init_func_name}();'.format(k,**v) for k,v in repl_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('uint32_t {find_victim_func_name}(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t);'.format(**r) for r in repl_data.values())
    retval += '\nuint32_t impl_replacement_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)\n{\n    '
    retval += 'uint32_t result = NUM_WAY;\n    '
    retval += '\n    '.join('if (repl_type[lg2(r{})]) result = {find_victim_func_name}(cpu, instr_id, set, current_set, ip, full_addr, type);'.format(k,**v) for k,v in repl_data.items())
    retval += '\n    return result;'
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {update_func_name}(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t);'.format(**r) for r in repl_data.values())
    retval += '\nvoid impl_replacement_update_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)\n{\n    '
    retval += '\n    '.join('if (repl_type[lg2(r{})]) {update_func_name}(cpu, set, way, full_addr, ip, victim_addr, type, hit);'.format(k,**v) for k,v in repl_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {final_func_name}();'.format(**r) for r in repl_data.values())
    retval += '\nvoid impl_replacement_final_stats()\n{\n    '
    retval += '\n    '.join('if (repl_type[lg2(r{})]) {final_func_name}();'.format(k,**v) for k,v in repl_data.items())
    retval += '\n}\n'
    retval += '\n'

    return retval

