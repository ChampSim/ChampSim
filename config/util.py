
def iter_system(system, name, key='lower_level'):
    while name in system:
        yield system[name]
        name = system[name].get(key)

