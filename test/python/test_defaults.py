import unittest

import config.defaults

class L1IPathTests(unittest.TestCase):
    def setUp(self):
        self.cpu = {'L1I': 'l1'}
        self.caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

    def test_cpp_defaults(self):
        path = config.defaults.list_defaults_for_core(self.cpu, self.caches)[0]
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_l1i',
            'l2': 'champsim::defaults::default_l2c',
            'l3': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_instr_cache(self):
        path = config.defaults.list_defaults_for_core(self.cpu, self.caches)[0]
        defs = {v['name']:v.get('_is_instruction_cache',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        path = config.defaults.list_defaults_for_core(self.cpu, self.caches)[0]
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

class JoiningL1IPathDefaultsTests(unittest.TestCase):
    def setUp(self):
        self.cores = ({'L1I': 'l1_0'},{'L1I': 'l1_1'})
        self.same_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_0'}
        }

        self.different_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l3_0'} # bypass L2
        }

    def test_cpp_defaults_joins_at_same_level(self):
        path = config.defaults.list_defaults(self.cores, self.same_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1i',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1i'
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_joins_at_different_level(self):
        path = config.defaults.list_defaults(self.cores, self.different_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1i',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1i'
        }
        self.assertDictEqual(defs, expected)

class L1DPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cpu = {'L1D': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[1]
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_l1d',
            'l2': 'champsim::defaults::default_l2c',
            'l3': 'champsim::defaults::default_llc'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cpu = {'L1D': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2', 'lower_level': 'l3'},
            'l3': {'name': 'l3'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[1]
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
            'l3': False,
        }
        self.assertDictEqual(defs, expected)

class JoiningL1DPathDefaultsTests(unittest.TestCase):
    def setUp(self):
        self.cores = ({'L1D': 'l1_0'},{'L1D': 'l1_1'})
        self.same_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_0'}
        }

        self.different_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0', 'lower_level': 'l3_0'},
            'l3_0': {'name': 'l3_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l3_0'} # bypass L2
        }

    def test_cpp_defaults_joins_at_same_level(self):
        path = config.defaults.list_defaults(self.cores, self.same_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1d',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1d'
        }
        self.assertDictEqual(defs, expected)

    def test_cpp_defaults_joins_at_different_level(self):
        path = config.defaults.list_defaults(self.cores, self.different_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_l1d',
            'l2_0': 'champsim::defaults::default_l2c',
            'l3_0': 'champsim::defaults::default_llc',
            'l1_1': 'champsim::defaults::default_l1d'
        }
        self.assertDictEqual(defs, expected)

class ITLBPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cpu = {'ITLB': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[2]
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_itlb',
            'l2': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cpu = {'ITLB': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[2]
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
        }
        self.assertDictEqual(defs, expected)

class JoiningITLBPathDefaultsTests(unittest.TestCase):
    def setUp(self):
        self.cores = ({'ITLB': 'l1_0'},{'ITLB': 'l1_1'})
        self.same_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_0'},
        }

    def test_cpp_defaults_joins_at_same_level(self):
        path = config.defaults.list_defaults(self.cores, self.same_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_itlb',
            'l2_0': 'champsim::defaults::default_stlb',
            'l1_1': 'champsim::defaults::default_itlb'
        }
        self.assertDictEqual(defs, expected)

class DTLBPathTests(unittest.TestCase):
    def test_cpp_defaults(self):
        cpu = {'DTLB': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[3]
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1': 'champsim::defaults::default_dtlb',
            'l2': 'champsim::defaults::default_stlb'
        }
        self.assertDictEqual(defs, expected)

    def test_first_level(self):
        cpu = {'DTLB': 'l1'}
        caches = {
            'l1': {'name': 'l1', 'lower_level': 'l2'},
            'l2': {'name': 'l2'}
        }

        path = config.defaults.list_defaults_for_core(cpu, caches)[3]
        defs = {v['name']:v.get('_first_level',False) for v in path}
        expected = {
            'l1': True,
            'l2': False,
        }
        self.assertDictEqual(defs, expected)

class JoiningDTLBPathDefaultsTests(unittest.TestCase):
    def setUp(self):
        self.cores = ({'DTLB': 'l1_0'},{'DTLB': 'l1_1'})
        self.same_level_join_caches = {
            'l1_0': {'name': 'l1_0', 'lower_level': 'l2_0'},
            'l2_0': {'name': 'l2_0'},
            'l1_1': {'name': 'l1_1', 'lower_level': 'l2_0'},
        }

    def test_cpp_defaults_joins_at_same_level(self):
        path = config.defaults.list_defaults(self.cores, self.same_level_join_caches)
        defs = {v['name']:v.get('_defaults') for v in path}
        expected = {
            'l1_0': 'champsim::defaults::default_dtlb',
            'l2_0': 'champsim::defaults::default_stlb',
            'l1_1': 'champsim::defaults::default_dtlb'
        }
        self.assertDictEqual(defs, expected)
