import unittest

import config.makefile

class EachInDictListTests(unittest.TestCase):

    def test_empty(self):
        a = {}
        self.assertEqual(list(config.makefile.each_in_dict_list(a)), [])

    def test_single(self):
        a = { 'a': [1,2,3,4] }
        self.assertEqual(list(config.makefile.each_in_dict_list(a)), [ ('a',1), ('a',2), ('a',3), ('a',4) ])

    def test_multiple(self):
        a = {
                'a': [1,2],
                'b': [3,4]
            }
        self.assertEqual(list(config.makefile.each_in_dict_list(a)), [ ('a',1), ('a',2), ('b',3), ('b',4) ])

