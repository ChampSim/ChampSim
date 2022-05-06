
def get_btb_data(module_name):
    retval = {}

    # Resolve BTB function names
    retval['btb_initialize'] = 'btb_' + module_name + '_initialize'
    retval['btb_update'] = 'btb_' + module_name + '_update'
    retval['btb_predict'] = 'btb_' + module_name + '_predict'

    retval['opts'] = (
    '-Dinitialize_btb=' + retval['btb_initialize'],
    '-Dupdate_btb=' + retval['btb_update'],
    '-Dbtb_prediction=' + retval['btb_predict']
    )

    return retval

def get_btb_string(btb_data):
    retval = ''

    for i,b in enumerate(btb_data):
        retval += f'constexpr static int t{b} = {i};\n'
    retval += '\n'

    retval += '\n'.join('void {btb_initialize}();'.format(**b) for b in btb_data.values())
    retval += '\nvoid impl_btb_initialize()\n{\n    '
    retval += '\n    '.join('if (btb_type[t{}]) {btb_initialize}();'.format(k,**b) for k,b in btb_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {btb_update}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in btb_data.values())
    retval += '\nvoid impl_update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)\n{\n    '
    retval += '\n    '.join('if (btb_type[t{}]) {btb_update}(ip, branch_target, taken, branch_type);'.format(k,**b) for k,b in btb_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('std::pair<uint64_t, uint8_t> {btb_predict}(uint64_t);'.format(**b) for b in btb_data.values())
    retval += '\nstd::pair<uint64_t, uint8_t> impl_btb_prediction(uint64_t ip)\n{\n    '
    retval += 'std::pair<uint64_t, uint8_t> result;\n    '
    retval += '\n    '.join('if (btb_type[t{}]) result = {btb_predict}(ip);'.format(k,**b) for k,b in btb_data.items())
    retval += '\n    return result;'
    retval += '\n}\n'
    retval += '\n'

    return retval

