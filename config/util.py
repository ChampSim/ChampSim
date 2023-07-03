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

def iter_system(system, name, key='lower_level'):
    '''
    Iterate through a dictionary system.

    The system is organized as a dictionary of { c['name']: c } for all c.
    Starting at the given name, generate a path through the system, traversing by the given key.
    Loops are not allowed, each element may be visited at most once.

    :param system: the system to be iterated through
    :param name: the key to start at
    :param key: the key that points to the next element
    '''
    hoisted = {**system}
    while name in hoisted:
        val = hoisted.pop(name)
        yield val
        name = val.get(key)

def wrap_list(attr):
    ''' Wrap the given element in a list, if it is not already a list '''
    if not isinstance(attr, list):
        attr = [attr]
    return attr

def chain(*dicts):
    '''
    Combine two or more dictionaries.
    Values that are dictionaries are merged recursively.
    Values that are lists are joined.

    Dictionaries given earlier in the parameter list have priority.
    :param dicts: the sequence to be chained
    '''
    def merge(merger, tname, lhs, rhs):
        return {k:merger(v, rhs[k]) for k,v in lhs.items() if isinstance(v, tname) and isinstance(rhs.get(k), tname)}

    def merge_dicts(lhs,rhs):
        dict_merges = merge(merge_dicts, dict, lhs, rhs)
        list_merges = merge(operator.concat, list, lhs, rhs)
        return dict(itertools.chain(rhs.items(), lhs.items(), dict_merges.items(), list_merges.items()))

    return functools.reduce(merge_dicts, dicts)

def extend_each(lhs,rhs):
    ''' For two dictionaries whose values are lists, join the values that have the same key. '''
    merges = {k: (*lhs[k],*rhs[k]) for k in lhs if k in rhs}
    return {**lhs, **rhs, **merges}

def subdict(whole_dict, keys):
    ''' Extract only the given keys from a dictionary. If they keys are not present, they are not defaulted. '''
    return {k:v for k,v in whole_dict.items() if k in keys}

def combine_named(*iterables):
    '''
    Collect a sequence of sequences of dictionaries by their 'name' parameter.
    Earlier parameters have priority over later parameters.
    '''
    iterable = sorted(itertools.chain(*iterables), key=operator.itemgetter('name'))
    iterable = itertools.groupby(iterable, key=operator.itemgetter('name'))
    return {name: chain(*dict_list) for name, dict_list in iterable}

def upper_levels_for(system, name, key='lower_level'):
    '''
    List all elements of the system who have the given element name under the given key.

    :param system: the system to be iterated through
    :param name: the key to start at
    :param key: the key that points to the next element
    '''
    default_itemgetter = operator.methodcaller('get', key, '')
    upper_levels = sorted(system, key=default_itemgetter)
    upper_levels = itertools.groupby(upper_levels, key=default_itemgetter)
    return next(filter(lambda kv: kv[0] == name, upper_levels))[1]

def propogate_down(path, key):
    '''
    Propogate the value of a key down a path of dictionaries.
    Later elements inherit the value from earlier elements, unless they have one themselves.

    :param path: an iterable of dictionary values
    :param key: they dictionary key to propogate
    '''
    value = None
    for new_value,chunk in itertools.groupby(path, key=operator.methodcaller('get', key)):
        if new_value is not None:
            yield from chunk
            value = new_value
        else:
            yield from ({ **element, key: value } for element in chunk)

def append_except_last(iterable, suffix):
    ''' Append a string to each element of the iterable except the last one. '''
    retval = None
    first = True
    for element in iterable:
        if not first:
            yield retval + suffix
        retval = element
        first = False

    if retval is not None:
        yield retval

def do_for_first(func, iterable):
    '''
    Evaluate the function for the first element in the iterable and yield it.
    Then yield the rest of the iterable.
    '''
    iterator = iter(iterable)
    first = next(iterator, None)
    if first is not None:
        yield func(first)
        yield from iterator
