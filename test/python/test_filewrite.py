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
