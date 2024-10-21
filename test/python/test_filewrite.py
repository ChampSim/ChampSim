import unittest
import operator

import config.filewrite

class FilesAreDifferentTests(unittest.TestCase):
    def test_identical(self):
        a = '''
        test
        test
        more test
        '''

        self.assertFalse(config.filewrite.files_are_different(a.splitlines(),a.splitlines()))

    def test_slightly_different(self):
        a = '''
        test
        test
        more test
        '''

        b = '''
        test
        tast
        more test
        '''

        self.assertTrue(config.filewrite.files_are_different(a.splitlines(),b.splitlines()))

    def test_very_different(self):
        a = '''
        test
        test
        more test
        '''

        b = '''
        Lorem
        Ipsum
        '''

        self.assertTrue(config.filewrite.files_are_different(a.splitlines(),b.splitlines()))

class FragmentTests(unittest.TestCase):
    def test_empty_fragment_is_empty(self):
        self.assertEqual(list(iter(config.filewrite.Fragment())), [])

    def test_fragments_take_lists(self):
        parts = [('a', ('aaa',)), ('b', ('bbb',))]
        self.assertEqual(list(iter(config.filewrite.Fragment(parts))), parts)

    def test_fragments_can_join(self):
        a_parts = [('a', ('aaa',)), ('b', ('bbb',))]
        b_parts = [('b', ('***',)), ('c', ('ccc',))]
        expected = [('a', ('aaa',)), ('b', ('bbb', '***')), ('c', ('ccc',))]

        a_frag = config.filewrite.Fragment(a_parts)
        b_frag = config.filewrite.Fragment(b_parts)
        self.assertEqual(list(iter(config.filewrite.Fragment.join(a_frag, b_frag))), expected)

    def test_joined_fragments_preserve_cxx_headers(self):
        a_parts = [('a.cc', (*config.filewrite.cxx_generated_warning(), 'aaa'))]
        b_parts = [('a.cc', (*config.filewrite.cxx_generated_warning(), 'bbb'))]
        expected = [('a.cc', (*config.filewrite.cxx_generated_warning(), 'aaa','bbb'))]

        a_frag = config.filewrite.Fragment(a_parts)
        b_frag = config.filewrite.Fragment(b_parts)
        self.assertEqual(list(iter(config.filewrite.Fragment.join(a_frag, b_frag))), expected)

    def test_joined_fragments_preserve_make_headers(self):
        a_parts = [('a.mk', (*config.filewrite.make_generated_warning(), 'aaa'))]
        b_parts = [('a.mk', (*config.filewrite.make_generated_warning(), 'bbb'))]
        expected = [('a.mk', (*config.filewrite.make_generated_warning(), 'aaa','bbb'))]

        a_frag = config.filewrite.Fragment(a_parts)
        b_frag = config.filewrite.Fragment(b_parts)
        self.assertEqual(list(iter(config.filewrite.Fragment.join(a_frag, b_frag))), expected)
