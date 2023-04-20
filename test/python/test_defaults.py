import unittest

import config.defaults

class L1IPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cores = ({'L1I': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_l1i',
            'l2': 'champsim::defaults::default_l2c',
            'l3': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_instr_cache(self):
        cores = ({'L1I': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_is_instruction_cache',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cores = ({'L1I': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_multi(self):
        cores = ({'L1I': 'l1_0'},{'L1I': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1', 'lower_level': 'l3_1'},
            'l3_1': {'name': 'l3_1'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1i',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1i',
            'l2_1': 'champsim::defaults::default_l2c',
            'l3_1': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_instr_cache_multi(self):
        cores = ({'L1I': 'l1_0'},{'L1I': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1', 'lower_level': 'l3_1'},
            'l3_1': {'name': 'l3_1'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_is_instruction_cache',False) for v in path}
        expected = {
            'l1_0': True,
            'l2_0': False,
            'l3_0': False,
            'l1_1': True,
            'l2_1': False,
            'l3_1': False,
        }
        self.assertDictEqual(defs, expected)

    def test_first_level_multi(self):
        cores = ({'L1I': 'l1_0'},{'L1I': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1', 'lower_level': 'l3_1'},
            'l3_1': {'name': 'l3_1'}
        }

        path = config.defaults.l1i_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1_0': True,
            'l2_0': False,
            'l3_0': False,
            'l1_1': True,
            'l2_1': False,
            'l3_1': False,
        }
        self.assertDictEqual(defs, expected)

class L1DPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cores = ({'L1D': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.l1d_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_l1d',
            'l2': 'champsim::defaults::default_l2c',
            'l3': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cores = ({'L1D': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.l1d_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_multi(self):
        cores = ({'L1D': 'l1_0'},{'L1D': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1', 'lower_level': 'l3_1'},
            'l3_1': {'name': 'l3_1'},
        }

        path = config.defaults.l1d_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1d',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1d',
            'l2_1': 'champsim::defaults::default_l2c',
            'l3_1': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level_multi(self):
        cores = ({'L1D': 'l1_0'},{'L1D': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1', 'lower_level': 'l3_1'},
            'l3_1': {'name': 'l3_1'},
        }

        path = config.defaults.l1d_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1_0': True,
            'l2_0': False,
            'l3_0': False,
            'l1_1': True,
            'l2_1': False,
            'l3_1': False,
        }
        self.assertDictEqual(defs, expected)

class ITLBPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cores = ({'ITLB': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.itlb_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_itlb',
            'l2': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cores = ({'ITLB': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.itlb_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_multi(self):
        cores = ({'ITLB': 'l1_0'},{'ITLB': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1'}
        }

        path = config.defaults.itlb_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_itlb',
            'l2_0': 'champsim::defaults::default_stlb',
            'l1_1': 'champsim::defaults::default_itlb',
            'l2_1': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level_multi(self):
        cores = ({'ITLB': 'l1_0'},{'ITLB': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1'}
        }

        path = config.defaults.itlb_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1_0': True,
            'l2_0': False,
            'l1_1': True,
            'l2_1': False,
        }
        self.assertDictEqual(defs, expected)

class DTLBPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cores = ({'DTLB': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.dtlb_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_dtlb',
            'l2': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cores = ({'DTLB': 'l1'},)
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.dtlb_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_multi(self):
        cores = ({'DTLB': 'l1_0'},{'DTLB': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1'}
        }

        path = config.defaults.dtlb_path(cores, caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_dtlb',
            'l2_0': 'champsim::defaults::default_stlb',
            'l1_1': 'champsim::defaults::default_dtlb',
            'l2_1': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level_multi(self):
        cores = ({'DTLB': 'l1_0'},{'DTLB': 'l1_1'})
        caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_1'},
            'l2_1': {'name': 'l2_1'}
        }

        path = config.defaults.dtlb_path(cores, caches)
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1_0': True,
            'l2_0': False,
            'l1_1': True,
            'l2_1': False,
        }
        self.assertDictEqual(defs, expected)

