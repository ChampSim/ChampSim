import sys, os
import unittest

import config.util

class UtilFunctionsTest(unittest.TestCase):

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

    def test_subdict_removes_keys(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b')), {'a':1, 'b':2})

    def test_subdict_does_not_fail_on_missing(self):
        self.assertEqual(config.util.subdict({'a':1, 'b':2, 'c':3}, ('a','b','d')), {'a':1, 'b':2})

    def test_combine_named_flat(self):
        a = [{'name': 'a', 'k': 1}]
        b = [{'name': 'a', 'k': 2}, {'name': 'b', 'k': 2}]
        self.assertEqual(config.util.combine_named(a,b), {'a': {'name': 'a', 'k': 1}, 'b': {'name': 'b', 'k': 2}})

