import unittest
import operator
import itertools
import tempfile
import subprocess
import os

import config.util

class ChainTests(unittest.TestCase):

    def test_chain_flat(self):
        a = {'a': 1}
        b = {'b': 2}
        self.assertEqual(config.util.chain(a,b), {'a': 1, 'b': 2});
        self.assertEqual(config.util.chain(b,a), {'a': 1, 'b': 2});

    def test_chain_overwrite(self):
        a = {'a': 1}
        b = {'a': 2}
        self.assertEqual(config.util.chain(a,b), {'a': 1});
        self.assertEqual(config.util.chain(b,a), {'a': 2});

    def test_chain_lists(self):
        a = {'a': [1,2], 'b': 'test'}
        b = {'a': [3,4]}
        self.assertEqual(config.util.chain(a,b), {'a': [1,2,3,4], 'b': 'test'});
        self.assertEqual(config.util.chain(b,a), {'a': [3,4,1,2], 'b': 'test'});

    def test_chain_dicts(self):
        a = {'a': {'a.a': 2}, 'b': 'test'}
        b = {'a': {'a.a': 3}}
        self.assertEqual(config.util.chain(a,b), {'a': {'a.a': 2}, 'b': 'test'});
        self.assertEqual(config.util.chain(b,a), {'a': {'a.a': 3}, 'b': 'test'});

    def test_modified_result_leaves_priors_unmodified(self):
        a = {'a': 1}
        b = {'a': 2}
        c = config.util.chain(a,b)
        c.update(a=5000)
        self.assertEqual(c, {'a': 5000})
        self.assertEqual(a, {'a': 1})
        self.assertEqual(b, {'a': 2})

class SubdictTests(unittest.TestCase):
    def test_subdict_removes_keys(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b')), {'a':1, 'b':2})

    def test_subdict_does_not_fail_on_missing(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b','d')), {'a':1, 'b':2})

    def test_subdict_invert(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b'), invert=True), {'c':3})

class CombineNamedTests(unittest.TestCase):
    def test_empty(self):
        self.assertEqual(len(config.util.combine_named()), 0)

    def test_combine_named_flat(self):
        a = [{'name': 'a', 'k': 1}]
        b = [{'name': 'a', 'k': 2}, {'name': 'b', 'k': 2}]
        self.assertEqual(config.util.combine_named(a,b), {'a': {'name': 'a', 'k': 1}, 'b': {'name': 'b', 'k': 2}})

    def test_values_without_names_are_skipped(self):
        a = [{'k': 1}]
        b = [{'name': 'a', 'k': 2}, {'name': 'b', 'k': 2}]
        self.assertEqual(config.util.combine_named(a,b), {'a': {'name': 'a', 'k': 2}, 'b': {'name': 'b', 'k': 2}})

class IterSystemTests(unittest.TestCase):
    def test_iter_system_all(self):
        system = {
                'a': {'next': 'b', 'id': 1},
                'b': {'next': 'c', 'id': 2},
                'c': {'id': 3},
                }
        self.assertEqual( list(map(operator.itemgetter('id'), config.util.iter_system(system, 'a', key='next'))), [1,2,3])

    def test_iter_system_notall(self):
        system = {
                'a': {'next': 'b', 'id': 1},
                'b': {'next': 'c', 'id': 2},
                'c': {'id': 3},
                'd': {'id': 4}
                }
        self.assertEqual( list(map(operator.itemgetter('id'), config.util.iter_system(system, 'a', key='next'))), [1,2,3])

    def test_iter_system_loop(self):
        system = {
                'a': {'next': 'b', 'id': 1},
                'b': {'next': 'c', 'id': 2},
                'c': {'next': 'a', 'id': 3},
                'd': {'id': 4}
                }
        self.assertEqual( list(map(operator.itemgetter('id'), config.util.iter_system(system, 'a', key='next'))), [1,2,3])
        self.assertEqual( list(map(operator.itemgetter('id'), config.util.iter_system(system, 'b', key='next'))), [2,3,1])

class WrapListTests(unittest.TestCase):
    def test_wrap_list(self):
        self.assertEqual(config.util.wrap_list([1,2]), [1,2])

    def test_wrap_nonlist(self):
        self.assertEqual(config.util.wrap_list(1), [1])

class ExtendEachTests(unittest.TestCase):

    def test_no_lists(self):
        a = {'a': 1}
        b = {'b': 2}
        self.assertEqual(config.util.extend_each(a,b), {'a': 1, 'b': 2});
        self.assertEqual(config.util.extend_each(b,a), {'a': 1, 'b': 2});

    def test_one_lists(self):
        a = {'a': 1}
        b = {'b': (2,3)}
        self.assertEqual(config.util.extend_each(a,b), {'a': 1, 'b': (2,3)});
        self.assertEqual(config.util.extend_each(b,a), {'a': 1, 'b': (2,3)});

    def test_one_lists(self):
        a = {'b': (1,4)}
        b = {'b': (2,3)}
        self.assertEqual(config.util.extend_each(a,b), {'b': (1,4,2,3)});
        self.assertEqual(config.util.extend_each(b,a), {'b': (2,3,1,4)});

class UpperLevelsForTests(unittest.TestCase):
    def test_upper_levels(self):
        system = {
                'a': {'next': 'c', 'id': 1},
                'b': {'next': 'c', 'id': 2},
                'c': {'next': 'a', 'id': 3}
                }
        self.assertEqual( list(map(operator.itemgetter('id'), config.util.upper_levels_for(system.values(), 'c', key='next'))), [1,2])

class PropogateDownTests(unittest.TestCase):
    def test_empty_path(self):
        self.assertEqual(list(config.util.propogate_down([], 'test')), [])

    def test_full_path_is_unaffected(self):
        path = [{ 'test': i } for i in range(8)]
        self.assertEqual(list(config.util.propogate_down(path, 'test')), path)

    def test_gaps_are_filled(self):
        path = [{ 'test': 1 }, {}, { 'test': 2 }, {}, {}]
        expected_result = [{ 'test': 1 }, { 'test': 1 }, { 'test': 2 }, { 'test': 2 }, { 'test': 2 }]
        self.assertEqual(list(config.util.propogate_down(path, 'test')), expected_result)

class CutTests(unittest.TestCase):
    def test_empty_gives_two_empty(self):
        for cutpoint in (1,-1):
            with self.subTest(n=cutpoint):
                testval = []
                head, tail = config.util.cut(testval, n=cutpoint)
                self.assertEqual(list(head), [])
                self.assertEqual(list(tail), [])

    def test_length_one_can_go_to_head(self):
        testval = ['teststring']
        head, tail = config.util.cut(testval, n=1)
        self.assertEqual(list(head), testval)
        self.assertEqual(list(tail), [])

    def test_length_one_can_go_to_tail(self):
        testval = ['teststring']
        head, tail = config.util.cut(testval, n=-1)
        self.assertEqual(list(head), [])
        self.assertEqual(list(tail), testval)

    def test_positive_count_that_exceeds_sends_all_to_head(self):
        testval = ['teststring']
        head, tail = config.util.cut(testval, n=2)
        self.assertEqual(list(head), testval)
        self.assertEqual(list(tail), [])

    def test_negative_count_that_exceeds_sends_all_to_tail(self):
        testval = ['teststring']
        head, tail = config.util.cut(testval, n=-2)
        self.assertEqual(list(head), [])
        self.assertEqual(list(tail), testval)

    def test_middle_splits(self):
        testval = ['teststringa', 'teststringb', 'teststringc']
        for cutpoint in (1,2,-1,-2):
            with self.subTest(n=cutpoint):
                head, tail = config.util.cut(testval, n=cutpoint)
                self.assertEqual(list(head), testval[:cutpoint])
                self.assertEqual(list(tail), testval[cutpoint:])

class AppendExceptLastTests(unittest.TestCase):
    def test_empty_does_not_append(self):
        testval = []
        result = list(config.util.append_except_last(testval, 'a'))
        self.assertEqual(result, testval)

    def test_length_one_does_not_append(self):
        testval = ['teststring']
        result = list(config.util.append_except_last(testval, 'a'))
        self.assertEqual(result, testval)

    def test_longer_length_appends(self):
        for length in (2,4,8,16):
            testval = ['teststring'] * length
            result = config.util.append_except_last(testval, 'a')
            expected = ['teststringa'] * (length-1) + ['teststring']
            for i,elem_pair in enumerate(itertools.zip_longest(result, expected, fillvalue=None)):
                with self.subTest(iterable_length=length, element_index=i):
                    self.assertEqual(*elem_pair)

class DoForFirstTests(unittest.TestCase):
    def test_empty_does_not_transform(self):
        testval = []
        result = list(config.util.do_for_first(lambda x: 'ABC', testval))
        self.assertEqual(result, [])

    def test_first_transforms(self):
        for length in (1,2,4,8,16):
            with self.subTest(length=length):
                testval = ['teststring'] * length
                result = list(config.util.do_for_first(lambda x: 'ABC', testval))
                expected = ['ABC'] + ['teststring'] * (length-1)
                self.assertEqual(result, expected)

class MultilineTests(unittest.TestCase):
    def test_empty_does_nothing(self):
        self.assertEqual(list(config.util.multiline([])), [])

    def test_shorter_than_length_yields_one_line(self):
        testval = ['test']
        self.assertEqual(list(config.util.multiline(testval, length=2)), ['test'])

    def test_longer_than_length_groups_elements(self):
        testval = ['testa', 'testb', 'testc']
        self.assertEqual(list(config.util.multiline(testval, length=2)), ['testa testb', 'testc'])

    def test_lines_can_indent(self):
        testval = ['testa', 'testb', 'testc']
        self.assertEqual(list(config.util.multiline(testval, length=2, indent=1)), ['testa testb', '  testc'])

    def test_line_terminators_are_added(self):
        testval = ['testa', 'testb', 'testc']
        self.assertEqual(list(config.util.multiline(testval, length=2, line_end='x')), ['testa testbx', 'testc'])


class YieldFromStar(unittest.TestCase):
    class Generator:
        def __init__(self, gen):
            self.gen = gen
            self.value = None

        def __iter__(self):
            self.value = yield from self.gen

    @staticmethod
    def empty_gen2():
        if False:
            yield None # never yield
        return 'empty_gen_first', 'empty_gen_second'

    @staticmethod
    def empty_gen3():
        if False:
            yield None # never yield
        return 'empty_gen_first', 'empty_gen_second', 'empty_gen_third'

    @staticmethod
    def identity_gen1(count):
        yield from range(count)
        return 'identity_first'

    @staticmethod
    def identity_gen2(count):
        yield from range(count)
        return 'identity_first', 'identity_second'

    def test_empty_collects_two(self):
        gen = YieldFromStar.Generator(config.util.yield_from_star(YieldFromStar.empty_gen2, (tuple(), tuple()), n=2))
        yielded = list(iter(gen))
        first, second = gen.value
        self.assertEqual(yielded, [])
        self.assertEqual(first, ['empty_gen_first', 'empty_gen_first'])
        self.assertEqual(second, ['empty_gen_second', 'empty_gen_second'])

    def test_empty_collects_three(self):
        gen = YieldFromStar.Generator(config.util.yield_from_star(YieldFromStar.empty_gen3, (tuple(), tuple()), n=3))
        yielded = list(iter(gen))
        first, second, third = gen.value
        self.assertEqual(yielded, [])
        self.assertEqual(first, ['empty_gen_first', 'empty_gen_first'])
        self.assertEqual(second, ['empty_gen_second', 'empty_gen_second'])
        self.assertEqual(third, ['empty_gen_third', 'empty_gen_third'])

    def test_identity_collects_one(self):
        gen = YieldFromStar.Generator(config.util.yield_from_star(YieldFromStar.identity_gen1, ((2,), (4,)), n=1))
        yielded = list(iter(gen))
        first = gen.value[0]
        self.assertEqual(yielded, [0,1,0,1,2,3])
        self.assertEqual(first, ['identity_first', 'identity_first'])

    def test_identity_collects_two(self):
        gen = YieldFromStar.Generator(config.util.yield_from_star(YieldFromStar.identity_gen2, ((2,), (4,)), n=2))
        yielded = list(iter(gen))
        first, second = gen.value
        self.assertEqual(yielded, [0,1,0,1,2,3])
        self.assertEqual(first, ['identity_first', 'identity_first'])
        self.assertEqual(second, ['identity_second', 'identity_second'])

class ExplodeTests(unittest.TestCase):
    def test_empty_list(self):
        given = { 'dog': 'rough collie', 'test': [] }
        expected = []
        evaluated = config.util.explode(given, 'test')
        self.assertEqual(expected, evaluated)

    def test_single_element_list(self):
        given = { 'dog': 'rough collie', 'test': ['good'] }
        expected = [ { 'test': 'good', 'dog': 'rough collie' } ]
        evaluated = config.util.explode(given, 'test')
        self.assertEqual(expected, evaluated)

    def test_multiple_element_list(self):
        given = { 'dog': 'rough collie', 'test': ['good', 'bad'] }
        expected = [ { 'test': 'good', 'dog': 'rough collie' },  { 'test': 'bad', 'dog': 'rough collie' } ]
        evaluated = config.util.explode(given, 'test')
        self.assertEqual(expected, evaluated)

    def test_modify_key(self):
        given = { 'dog': 'rough collie', 'test': ['good'] }
        expected = [ { 'newkey': 'good', 'dog': 'rough collie' } ]
        evaluated = config.util.explode(given, 'test', 'newkey')
        self.assertEqual(expected, evaluated)

class PathPartsTests(unittest.TestCase):
    def test_empty(self):
        given = ''
        expected = tuple()
        evaluated = tuple(config.util.path_parts(given))
        self.assertEqual(expected, evaluated)

    def test_no_dir(self):
        given = 'cat'
        expected = ('cat',)
        evaluated = tuple(config.util.path_parts(given))
        self.assertEqual(expected, evaluated)

    def test_two_parts(self):
        given = 'cat/dog'
        expected = ('cat', 'dog')
        evaluated = tuple(config.util.path_parts(given))
        self.assertEqual(expected, evaluated)

    def test_three_parts(self):
        given = 'cat/dog/cow'
        expected = ('cat', 'dog', 'cow')
        evaluated = tuple(config.util.path_parts(given))
        self.assertEqual(expected, evaluated)

class PathAncestorsTests(unittest.TestCase):
    def test_empty(self):
        given = ''
        expected = tuple()
        evaluated = tuple(config.util.path_ancestors(given))
        self.assertEqual(expected, evaluated)

    def test_no_dir(self):
        given = 'cat'
        expected = ('cat',)
        evaluated = tuple(config.util.path_ancestors(given))
        self.assertEqual(expected, evaluated)

    def test_two_parts(self):
        given = 'cat/dog'
        expected = ('cat', 'cat/dog')
        evaluated = tuple(config.util.path_ancestors(given))
        self.assertEqual(expected, evaluated)

    def test_three_parts(self):
        given = 'cat/dog/cow'
        expected = ('cat', 'cat/dog', 'cat/dog/cow')
        evaluated = tuple(config.util.path_ancestors(given))
        self.assertEqual(expected, evaluated)

class BatchTests(unittest.TestCase):
    def test_empty(self):
        given = tuple()
        expected = tuple()
        evaluated = tuple(config.util.batch(given, 2))
        self.assertEqual(expected, evaluated)

    def test_single_batch(self):
        given = ('cat', 'dog')
        expected = (('cat', 'dog'),)
        evaluated = tuple(config.util.batch(given, 2))
        self.assertEqual(expected, evaluated)

    def test_multiple_batch(self):
        given = ('cat', 'dog', 'pig', 'cow')
        expected = (('cat', 'dog'), ('pig', 'cow'))
        evaluated = tuple(config.util.batch(given, 2))
        self.assertEqual(expected, evaluated)

    def test_nonuniform(self):
        given = ('cat', 'dog', 'pig')
        expected = (('cat', 'dog'), ('pig',))
        evaluated = tuple(config.util.batch(given, 2))
        self.assertEqual(expected, evaluated)

class SlidingTests(unittest.TestCase):
    def test_empty(self):
        given = tuple()
        expected = tuple()
        evaluated = tuple(config.util.sliding(given, 2))
        self.assertEqual(expected, evaluated)

    def test_underflow(self):
        given = ('cat',)
        expected = tuple()
        evaluated = tuple(config.util.sliding(given, 2))
        self.assertEqual(expected, evaluated)

    def test_single_window(self):
        given = ('cat', 'dog')
        expected = (('cat', 'dog'),)
        evaluated = tuple(config.util.sliding(given, 2))
        self.assertEqual(expected, evaluated)

    def test_multiple_batch(self):
        given = ('cat', 'dog', 'pig')
        expected = (('cat', 'dog'), ('dog', 'pig'))
        evaluated = tuple(config.util.sliding(given, 2))
        self.assertEqual(expected, evaluated)

    def test_longer_window(self):
        given = ('cat', 'dog', 'pig', 'cow')
        expected = (('cat', 'dog', 'pig'), ('dog', 'pig', 'cow'))
        evaluated = tuple(config.util.sliding(given, 3))
        self.assertEqual(expected, evaluated)
