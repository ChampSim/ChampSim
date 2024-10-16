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
import collections
import os

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
    ''' Wrap the given element in a list, if it is not already a list. '''
    if not isinstance(attr, list):
        attr = [attr]
    return attr

def collect(iterable, key_func, join_func):
    ''' Perform the "sort->groupby" idiom on an iterable, grouping according to the join_func. '''
    intern_iterable = sorted(iterable, key=key_func)
    intern_iterable = itertools.groupby(intern_iterable, key=key_func)
    return (join_func(it[1]) for it in intern_iterable)

def chain(*dicts):
    '''
    Combine two or more dictionaries.
    Values that are dictionaries are merged recursively.
    Values that are lists are joined.

    Dictionaries given earlier in the parameter list have priority.

    >>> chain({ 'a': 1 }, { 'b': 2 })
    { 'a': 1, 'b': 2 }
    >>> chain({ 'a': 1 }, { 'a': 2 })
    { 'a': 1 }
    >>> chain({ 'd': { 'a': 1 } }, { 'd': { 'b': 2 } })
    { 'd': { 'a': 1, 'b': 2 } }

    :param dicts: the sequence to be chained
    '''
    def merge(merger, tname, lhs, rhs):
        return {k:merger(v, rhs[k]) for k,v in lhs.items() if isinstance(v, tname) and isinstance(rhs.get(k), tname)}

    def merge_dicts(lhs,rhs):
        dict_merges = merge(merge_dicts, dict, lhs, rhs)
        list_merges = merge(operator.concat, list, lhs, rhs)
        return dict(itertools.chain(rhs.items(), lhs.items(), dict_merges.items(), list_merges.items()))

    return functools.reduce(merge_dicts, dicts, {})

def star(func):
    ''' Convert a function object that takes a starred parameter into one that takes an iterable parameter. '''
    def result(args):
        return func(*args)
    return result

def extend_each(lhs,rhs):
    ''' For two dictionaries whose values are lists, join the values that have the same key. '''
    merges = {k: (*lhs[k],*rhs[k]) for k in lhs if k in rhs}
    return {**lhs, **rhs, **merges}

def subdict(whole_dict, keys, invert=False):
    ''' Extract only the given keys from a dictionary. If they keys are not present, they are not defaulted. '''
    return {k:v for k,v in whole_dict.items() if (k in keys) != invert}

def combine_named(*iterables):
    '''
    Collect a sequence of sequences of dictionaries by their 'name' parameter.
    Earlier parameters have priority over later parameters.
    '''
    key_func = operator.methodcaller('get', 'name', '')
    items = ((key_func(d), d) for d in collect(itertools.chain(*iterables), key_func, star(chain)))
    return dict(filter(operator.itemgetter(0), items))

def upper_levels_for(system, name, key='lower_level'):
    '''
    List all elements of the system who have the given element name under the given key.

    :param system: the system to be iterated through
    :param name: the key to start at
    :param key: the key that points to the next element
    '''
    default_itemgetter = operator.methodcaller('get', key, '')
    return next(filter(lambda v: default_itemgetter(v[0]) == name, collect(system, default_itemgetter, tuple)))

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

def cut(iterable, n=-1):
    '''
    Split an iterable into a head and a tail. The head should be completely consumed before the tail is accesssed.

    :param iterable: An iterable
    :param n: The length of the head or, if the value is negative, the length of the tail.
    '''
    it = iter(iterable)
    if n >= 0:
        return itertools.islice(it, n), it

    tail = collections.deque(itertools.islice(it, -1*n))
    def head_iterator():
        for elem in it:
            yield tail.popleft()
            tail.append(elem)
    def tail_iterator():
        yield from tail

    return head_iterator(), tail_iterator()

def append_except_last(iterable, suffix):
    ''' Concatenate a suffix to each element of the iterable except the last one. '''
    head, tail = cut(iterable, n=-1)
    yield from map(operator.concat, head, itertools.repeat(suffix))
    yield from tail

def do_for_first(func, iterable):
    '''
    Evaluate the function for the first element in the iterable and yield it.
    Then yield the rest of the iterable.
    '''
    head, tail = cut(iterable, n=1)
    yield from map(func, head)
    yield from tail

def batch(it, n):
    ''' A backport of itertools.batch(). '''
    it = iter(it)
    val = tuple(itertools.islice(it, n))
    while val:
        yield val
        val = tuple(itertools.islice(it, n))

def multiline(long_line, length=1, indent=0, line_end=None):
    ''' Split a long string into lines with n words '''
    grouped = map(' '.join, batch(long_line, length))
    lines = append_except_last(grouped, line_end or '')
    indentation = itertools.chain(('',), itertools.repeat('  '*indent))
    yield from (i+l for i,l in zip(indentation,lines))

def yield_from_star(gen, args, n=2):
    '''
    Python generators can return values when they are finished.
    This adaptor yields the values from the generators and collects the returned values into a list.
    '''
    retvals = [[] for _ in range(n)]
    for argument in args:
        instance_retval = yield from gen(*argument)
        if not isinstance(instance_retval, tuple):
            instance_retval = (instance_retval,)
        for seq,return_value in zip(retvals, instance_retval):
            seq.append(return_value)
    return retvals

def explode(value, in_key, out_key=None):
    '''
    Convert a dictionary with a list member to a list with dictionary members.
    :param value: the dictionary to be extracted
    :param in_key: the key holding the list
    :param out_key: the key to distinguish the resulting list elements
    '''
    if out_key is None:
        out_key = in_key
    extracted = value.pop(in_key)
    return [ { out_key: extracted_element, **value } for extracted_element in extracted ]

def path_parts(path):
    ''' Yield the components of a path, as if by repeated applications of os.path.split(). '''
    if not path:
        return
    head, tail = os.path.split(path)
    yield from path_parts(head)
    yield tail

def path_ancestors(path):
    ''' Yield all directories that are ancestors of the path. '''
    yield from itertools.accumulate(path_parts(path), os.path.join)

def sliding(iterable, n):
    ''' A backport of itertools.sliding() '''
    it = iter(iterable)
    window = collections.deque(itertools.islice(it, n-1), maxlen=n)
    for element in it:
        window.append(element)
        yield tuple(window)
