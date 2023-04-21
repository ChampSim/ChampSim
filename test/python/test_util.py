import unittest
import operator

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

class SubdictTests(unittest.TestCase):
    def test_subdict_removes_keys(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b')), {'a':1, 'b':2})

    def test_subdict_does_not_fail_on_missing(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b','d')), {'a':1, 'b':2})

class CombineNamedTests(unittest.TestCase):
    def test_combine_named_flat(self):
        a = [{'name': 'a', 'k': 1}]
        b = [{'name': 'a', 'k': 2}, {'name': 'b', 'k': 2}]
        self.assertEqual(config.util.combine_named(a,b), {'a': {'name': 'a', 'k': 1}, 'b': {'name': 'b', 'k': 2}})

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

