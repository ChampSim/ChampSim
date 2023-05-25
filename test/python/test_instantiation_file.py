import unittest

import config.instantiation_file

class VectorStringgTest(unittest.TestCase):

    def test_empty_list(self):
        self.assertEqual(config.instantiation_file.vector_string([]), '{}');

    def test_list_with_one(self):
        self.assertEqual(config.instantiation_file.vector_string(['a']), 'a');

    def test_list_with_two(self):
        self.assertEqual(config.instantiation_file.vector_string(['a','b']), '{a, b}');

