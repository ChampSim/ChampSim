
def read_element_name(cpu, elem):
    return cpu.get(elem) if isinstance(cpu.get(elem), str) else cpu.get(elem,{}).get('name', cpu['name']+'_'+elem)

def iter_system(system, name, key='lower_level'):
    while name in system:
        yield system[name]
        name = system[name].get(key)

def wrap_list(attr):
    if not isinstance(attr, list):
        attr = [attr]
    return attr

