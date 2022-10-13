import itertools
import functools
import operator

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

def chain(*dicts, merge_funcs=dict()):
    def merge_dicts(x,y):
        merges = {k:merge_dicts(v, y[k]) for k,v in x.items() if isinstance(v, dict) and isinstance(y.get(k), dict)}
        merges.update({k:f(x.get(k),y.get(k)) for k,f in merge_funcs.items()})
        return { **y, **x, **merges }

    return functools.reduce(merge_dicts, dicts)

def combine_named(*iterables):
    iterable = sorted(itertools.chain(*iterables), key=operator.itemgetter('name'))
    iterable = itertools.groupby(iterable, key=operator.itemgetter('name'))
    return {kv[0]: chain(*kv[1]) for kv in iterable}

