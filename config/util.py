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
import functools
import operator

def read_element_name(cpu, elem):
    return cpu.get(elem) if isinstance(cpu.get(elem), str) else cpu.get(elem,{}).get('name', cpu['name']+'_'+elem)

def iter_system(system, name, key='lower_level'):
    hoisted = dict(**system)
    while name in hoisted:
        val = hoisted.pop(name)
        yield val
        name = val.get(key)

def wrap_list(attr):
    if not isinstance(attr, list):
        attr = [attr]
    return attr

def chain(*dicts):
    def merge_dicts(x,y):
        dict_merges = {k:merge_dicts(v, y[k]) for k,v in x.items() if isinstance(v, dict) and isinstance(y.get(k), dict)}
        list_merges = {k:(v + y[k]) for k,v in x.items() if isinstance(v, list) and isinstance(y.get(k), list)}
        return dict(itertools.chain(y.items(), x.items(), dict_merges.items(), list_merges.items()))

    return functools.reduce(merge_dicts, dicts)

def extend_each(x,y):
    merges = {k: (*x[k],*y[k]) for k in x if k in y}
    return {**x, **y, **merges}

def subdict(d, keys):
    return {k:v for k,v in d.items() if k in keys}

def combine_named(*iterables):
    iterable = sorted(itertools.chain(*iterables), key=operator.itemgetter('name'))
    iterable = itertools.groupby(iterable, key=operator.itemgetter('name'))
    return {kv[0]: chain(*kv[1]) for kv in iterable}

# Assign defaults that are unique per core
def upper_levels_for(system, name, key='lower_level'):
    finder = lambda x: x.get(key, '')
    upper_levels = sorted(system, key=finder)
    upper_levels = itertools.groupby(upper_levels, key=finder)
    return next(filter(lambda kv: kv[0] == name, upper_levels))[1]

