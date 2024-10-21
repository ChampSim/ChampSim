import unittest
import os
import tempfile
import subprocess

import config.cxx

class CheckCompilesTests(unittest.TestCase):
    def test_empty(self):
        self.assertTrue(config.cxx.check_compiles(tuple()))

    def test_instantiate_int(self):
        self.assertTrue(config.cxx.check_compiles(('int test{0};',)))

    def test_instantiate_int_with_chararray(self):
        self.assertFalse(config.cxx.check_compiles(('int test{"Hi, Mom!"};',)))

class FunctionTests(unittest.TestCase):
    def do_build(self, function):
        with tempfile.TemporaryDirectory() as dtemp:
            fname = os.path.join(dtemp, 'test.cc')
            with open(fname, 'wt') as wfp:
                for l in function:
                    print(l, file=wfp)
            subprocess.run(['c++', fname, '-o', os.path.join(dtemp, 'test')])

    def test_main(self):
        self.do_build(config.cxx.function('main', [], rtype='int'))

    def test_main_with_args(self):
        self.do_build(config.cxx.function('main', [], args=[('int', 'argc'), ('char**', 'argv')], rtype='int'))

    def test_body(self):
        self.do_build(config.cxx.function('main', ['return 0;'], rtype='int'))

class StructTests(unittest.TestCase):
    def do_build(self, struct):
        with tempfile.TemporaryDirectory() as dtemp:
            fname = os.path.join(dtemp, 'test.cc')
            with open(fname, 'wt') as wfp:
                for l in struct:
                    print(l, file=wfp)

                # main function
                for l in config.cxx.function('main', [], rtype='int'):
                    print(l, file=wfp)
            subprocess.run(['c++', fname, '-o', os.path.join(dtemp, 'test')])

    def test_empty(self):
        self.do_build(config.cxx.struct('A', []))

    def test_super(self):
        base = config.cxx.struct('A', [])
        deriv = config.cxx.struct('B', [], superclass='A')
        self.do_build([*base, *deriv])
