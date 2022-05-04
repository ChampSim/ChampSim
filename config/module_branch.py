
def get_branch_data(module_name):
    retval = {}

    # Resolve branch predictor function names
    retval['bpred_initialize'] = 'bpred_' + module_name + '_initialize'
    retval['bpred_last_result'] = 'bpred_' + module_name + '_last_result'
    retval['bpred_predict'] = 'bpred_' + module_name + '_predict'

    retval['opts'] = (
    '-Dinitialize_branch_predictor=' + retval['bpred_initialize'],
    '-Dlast_branch_result=' + retval['bpred_last_result'],
    '-Dpredict_branch=' + retval['bpred_predict']
    )

    return retval

def get_branch_string(branch_data):
    retval = ''

    for i,b in enumerate(branch_data):
        retval += f'constexpr static std::size_t b{b} = 1 << {i};\n'
    retval += '\n'

    retval += '\n'.join('void {bpred_initialize}();'.format(**b) for b in branch_data.values())
    retval += '\nvoid impl_branch_predictor_initialize()\n{\n    '
    retval += '\n    '.join('if (bpred_type[b{}]) {bpred_initialize}();'.format(k,**b) for k,b in branch_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('void {bpred_last_result}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in branch_data.values())
    retval += '\nvoid impl_last_branch_result(uint64_t ip, uint64_t target, uint8_t taken, uint8_t branch_type)\n{\n    '
    retval += '\n    '.join('if (bpred_type[b{}]) {bpred_last_result}(ip, target, taken, branch_type);'.format(k,**b) for k,b in branch_data.items())
    retval += '\n}\n'
    retval += '\n'

    retval += '\n'.join('uint8_t {bpred_predict}(uint64_t, uint64_t, uint8_t, uint8_t);'.format(**b) for b in branch_data.values())
    retval += '\nuint8_t impl_predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)\n{\n    '
    retval += 'std::bitset<NUM_BRANCH_MODULES> result;\n    '
    retval += '\n    '.join('if (bpred_type[b{0}]) result[b{0}] = {bpred_predict}(ip, predicted_target, always_taken, branch_type);'.format(k,**b) for k,b in branch_data.items())
    retval += '\n    return result.any();'
    retval += '\n}\n\n'

    return retval

