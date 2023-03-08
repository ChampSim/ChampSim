import unittest

import config.parse

class ExecutableNameTests(unittest.TestCase):

    def test_last_executable_takes_priority(self):
        a = { 'executable_name': 'a' }
        b = { 'executable_name': 'b' }
        self.assertEqual(config.parse.executable_name(a,b), b['executable_name'])

    def test_combine_names(self):
        a = { 'name': 'a' }
        b = { 'name': 'b' }
        self.assertEqual(config.parse.executable_name(a,b), 'champsim_a_b')

    def test_combine_names_skips_missing(self):
        a = {}
        b = { 'name': 'b' }
        self.assertEqual(config.parse.executable_name(a,b), 'champsim_b')
        self.assertEqual(config.parse.executable_name(b,a), 'champsim_b')

    def test_combine_names_empty(self):
        a = {}
        b = {}
        self.assertEqual(config.parse.executable_name(a,b), 'champsim')
        self.assertEqual(config.parse.executable_name(b,a), 'champsim')

    def test_executable_over_names(self):
        a = { 'name': 'a' }
        b = { 'name': 'b', 'executable_name': 'b_exec' }
        self.assertEqual(config.parse.executable_name(a,b), 'b_exec')
        self.assertEqual(config.parse.executable_name(b,a), 'b_exec')

class DuplicateToLengthTests(unittest.TestCase):

    def test_length(self):
        for i in range(1,10,2):
            with self.subTest(i=i):
                self.assertEqual(len(config.parse.duplicate_to_length([{}], i)), i)

    def test_duplication(self):
        a = { 'name': 'a' }
        self.assertEqual(config.parse.duplicate_to_length([a], 2), [a]*2)
        self.assertEqual(config.parse.duplicate_to_length([a], 4), [a]*4)
        self.assertEqual(config.parse.duplicate_to_length([a], 8), [a]*8)

    def test_grouped_duplication(self):
        a = { 'name': 'a' }
        b = { 'name': 'b' }
        self.assertEqual(config.parse.duplicate_to_length([a,b], 4), [a,a,b,b])
        self.assertEqual(config.parse.duplicate_to_length([a,b], 5), [a,a,a,b,b])
        self.assertEqual(config.parse.duplicate_to_length([a,b], 6), [a,a,a,b,b,b])

