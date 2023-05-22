import unittest
import itertools

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

    def test_truncate(self):
        a = { 'name': 'a' }
        b = { 'name': 'b' }
        c = { 'name': 'c' }
        self.assertEqual(config.parse.duplicate_to_length([a,b,c], 2), [a,b])

class FilterInaccessibleTests(unittest.TestCase):

    def test_empty_system(self):
        self.assertEqual(len(config.parse.filter_inaccessible({}, ['a', 'b'])), 0)

    def test_empty_roots(self):
        system = {
                'a' : { 'name': 'a', 'lower_level': 'b' },
                'b' : { 'name': 'b', 'lower_level': 'c' },
                'c' : { 'name': 'c', 'lower_level': 'd' }
                }
        self.assertEqual(len(config.parse.filter_inaccessible(system, [])), 0)

    def test_coincident_roots(self):
        system = {
                'a' : { 'name': 'a', 'lower_level': 'b' },
                'b' : { 'name': 'b', 'lower_level': 'c' },
                'c' : { 'name': 'c', 'lower_level': 'd' }
                }
        result = config.parse.filter_inaccessible(system, ['a','b'])
        self.assertCountEqual(system, result)

    def test_coincident_roots(self):
        system = {
                'a' : { 'name': 'a', 'lower_level': 'b' },
                'b' : { 'name': 'b', 'lower_level': 'c' },
                'c' : { 'name': 'c', 'lower_level': 'd' },
                'orphan' : { 'name': 'orphan', 'lower_level': 'd' }
                }
        result = config.parse.filter_inaccessible(system, ['a'])
        self.assertEqual(len(result), 3)
        self.assertIn('a', result)
        self.assertIn('b', result)
        self.assertIn('c', result)

class PassthroughContext:
    def find(self, module):
        return {'name': module, 'fname': 'xxyzzy/'+module, '_is_instruction_prefetcher': module.endswith('_instr')}

    def find_all(self):
        return [] # FIXME

class ApplyDefaultsTests(unittest.TestCase):

    def test_end_of_instruction_path_is_pmem(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1d' },
            { 'name': 'test_l1i', 'lower_level': 'test_l2c' },
            { 'name': 'test_l2c', 'lower_level': 'test_l3c' },
            { 'name': 'test_l3c' },
            { 'name': 'test_itlb' },
            { 'name': 'test_dtlb' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw'
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        caches = result[0]['caches']
        self.assertEqual('DRAM', caches[[c['name'] for c in caches].index('test_l3c')]['lower_level'])

    def test_end_of_data_path_is_pmem(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i' },
            { 'name': 'test_l1d', 'lower_level': 'test_l2c' },
            { 'name': 'test_l2c', 'lower_level': 'test_l3c' },
            { 'name': 'test_l3c' },
            { 'name': 'test_itlb' },
            { 'name': 'test_dtlb' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw'
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        caches = result[0]['caches']
        self.assertEqual('DRAM', caches[[c['name'] for c in caches].index('test_l3c')]['lower_level'])

    def test_instruction_caches_have_instruction_prefetchers(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i' },
            { 'name': 'test_l1d' },
            { 'name': 'test_itlb' },
            { 'name': 'test_dtlb' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw'
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        cache_names = [core['L1I'] for core in result[0]['cores']]
        caches = result[0]['caches']

        instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
        is_inst_data = {c['name']:[d['_is_instruction_prefetcher'] for d in c['_prefetcher_data']] for c in caches if c['name'] in cache_names}
        self.assertEqual(is_inst_data, {n:[True] for n in cache_names})

    def test_instruction_and_data_caches_need_translation(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i' },
            { 'name': 'test_l1d' },
            { 'name': 'test_itlb' },
            { 'name': 'test_dtlb' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw'
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        cache_names = [core['L1I'] for core in result[0]['cores']] + [core['L1D'] for core in result[0]['cores']]
        caches = result[0]['caches']

        filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
        tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

        self.assertEqual(tlb_names, {c:True for c in tlb_names.keys()})

    def test_tlbs_do_not_need_translation(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i' },
            { 'name': 'test_l1d' },
            { 'name': 'test_itlb' },
            { 'name': 'test_dtlb' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw'
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        cache_names = [core['ITLB'] for core in result[0]['cores']] + [core['DTLB'] for core in result[0]['cores']]
        caches = result[0]['caches']

        filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
        tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

        self.assertEqual(tlb_names, {c:False for c in tlb_names.keys()})

    def test_caches_inherit_core_frequency_1core(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i', 'lower_level': 'DRAM' },
            { 'name': 'test_l1d', 'lower_level': 'DRAM' },
            { 'name': 'test_itlb', 'lower_level': 'test_ptw' },
            { 'name': 'test_dtlb', 'lower_level': 'test_ptw' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw', 'lower_level': 'test_l1d' }
        ]}

        cores = [{
            'name': 'test_core',
            'L1I': 'test_l1i',
            'L1D': 'test_l1d',
            'ITLB': 'test_itlb',
            'DTLB': 'test_dtlb',
            'PTW': 'test_ptw',
            'frequency': 23
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        expected_frequencies = {core[cache_name]: core['frequency'] for core, cache_name in itertools.product(result[0]['cores'], ('L1I', 'L1D', 'ITLB', 'DTLB'))}
        actual_frequencies = {v['name']: v.get('frequency') for v in result[0]['caches']}
        self.assertEqual(expected_frequencies, actual_frequencies)

    def test_caches_inherit_core_frequency_2core(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i_0', 'lower_level': 'DRAM' },
            { 'name': 'test_l1d_0', 'lower_level': 'DRAM' },
            { 'name': 'test_itlb_0', 'lower_level': 'test_ptw_0' },
            { 'name': 'test_dtlb_0', 'lower_level': 'test_ptw_0' },
            { 'name': 'test_l1i_1', 'lower_level': 'DRAM' },
            { 'name': 'test_l1d_1', 'lower_level': 'DRAM' },
            { 'name': 'test_itlb_1', 'lower_level': 'test_ptw_1' },
            { 'name': 'test_dtlb_1', 'lower_level': 'test_ptw_1' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw_0', 'lower_level': 'test_l1d_0' },
            { 'name': 'test_ptw_1', 'lower_level': 'test_l1d_1' }
        ]}

        cores = [{
            'name': 'test_core_0',
            'L1I': 'test_l1i_0',
            'L1D': 'test_l1d_0',
            'ITLB': 'test_itlb_0',
            'DTLB': 'test_dtlb_0',
            'PTW': 'test_ptw_0',
            'frequency': 23
        },{
            'name': 'test_core_1',
            'L1I': 'test_l1i_1',
            'L1D': 'test_l1d_1',
            'ITLB': 'test_itlb_1',
            'DTLB': 'test_dtlb_1',
            'PTW': 'test_ptw_1',
            'frequency': 54
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        expected_frequencies = {core[cache_name]: core['frequency'] for core, cache_name in itertools.product(result[0]['cores'], ('L1I', 'L1D', 'ITLB', 'DTLB'))}
        actual_frequencies = {v['name']: v.get('frequency') for v in result[0]['caches']}
        self.assertEqual(expected_frequencies, actual_frequencies)

    def test_shared_caches_inherit_maximum_frequency(self):
        caches = {v['name']: v for v in [
            { 'name': 'test_l1i_0', 'lower_level': 'LLC' },
            { 'name': 'test_l1d_0', 'lower_level': 'LLC' },
            { 'name': 'test_itlb_0', 'lower_level': 'test_ptw_0' },
            { 'name': 'test_dtlb_0', 'lower_level': 'test_ptw_0' },
            { 'name': 'test_l1i_1', 'lower_level': 'LLC' },
            { 'name': 'test_l1d_1', 'lower_level': 'LLC' },
            { 'name': 'test_itlb_1', 'lower_level': 'test_ptw_1' },
            { 'name': 'test_dtlb_1', 'lower_level': 'test_ptw_1' },
            { 'name': 'LLC', 'lower_level': 'DRAM' }
        ]}

        ptws = {v['name']: v for v in [
            { 'name': 'test_ptw_0', 'lower_level': 'test_l1d_0' },
            { 'name': 'test_ptw_1', 'lower_level': 'test_l1d_1' }
        ]}

        cores = [{
            'name': 'test_core_0',
            'L1I': 'test_l1i_0',
            'L1D': 'test_l1d_0',
            'ITLB': 'test_itlb_0',
            'DTLB': 'test_dtlb_0',
            'PTW': 'test_ptw_0',
            'frequency': 23
        },{
            'name': 'test_core_1',
            'L1I': 'test_l1i_1',
            'L1D': 'test_l1d_1',
            'ITLB': 'test_itlb_1',
            'DTLB': 'test_dtlb_1',
            'PTW': 'test_ptw_1',
            'frequency': 54
        }]

        result = config.parse.apply_defaults_in_context({}, cores, caches, ptws, {}, {}, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        expected_frequencies = {core[cache_name]: core['frequency'] for core, cache_name in itertools.product(result[0]['cores'], ('L1I', 'L1D', 'ITLB', 'DTLB'))}
        expected_frequencies.update(LLC=max(c['frequency'] for c in result[0]['cores']))
        actual_frequencies = {v['name']: v.get('frequency') for v in result[0]['caches']}
        self.assertEqual(expected_frequencies, actual_frequencies)

class EnvironmentParseTests(unittest.TestCase):

    def setUp(self):
        self.minimal_config = (
            [{
                'name': 'test_core',
                'L1I': 'test_l1i',
                'L1D': 'test_l1d',
                'ITLB': 'test_itlb',
                'DTLB': 'test_dtlb',
                'PTW': 'test_ptw'
            }],
            { v['name']: v for v in [
                { 'name': 'test_l1i' },
                { 'name': 'test_l1d' },
                { 'name': 'test_itlb' },
                { 'name': 'test_dtlb' }
            ]},
            {v['name']: v for v in [
                { 'name': 'test_ptw' }
            ]},
            {},
            {}
        )

    def test_cc_passes_through(self):
        test_config = { 'CC': 'cc' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

    def test_cxx_passes_through(self):
        test_config = { 'CXX': 'cxx' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

    def test_cppflags_passes_through(self):
        test_config = { 'CPPFLAGS': 'cppflags' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

    def test_cxxflags_passes_through(self):
        test_config = { 'CXXFLAGS': 'cxxflags' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

    def test_ldflags_passes_through(self):
        test_config = { 'LDFLAGS': 'ldflags' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

    def test_ldlibs_passes_through(self):
        test_config = { 'LDLIBS': 'ldlibs' }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config, result[3])

class ConfigRootPassthroughParseTests(unittest.TestCase):

    def setUp(self):
        self.minimal_config = (
            [{
                'name': 'test_core',
                'L1I': 'test_l1i',
                'L1D': 'test_l1d',
                'ITLB': 'test_itlb',
                'DTLB': 'test_dtlb',
                'PTW': 'test_ptw'
            }],
            { v['name']: v for v in [
                { 'name': 'test_l1i' },
                { 'name': 'test_l1d' },
                { 'name': 'test_itlb' },
                { 'name': 'test_dtlb' }
            ]},
            {v['name']: v for v in [
                { 'name': 'test_ptw' }
            ]},
            {},
            {}
        )

    def test_block_size_passes_through(self):
        test_config = { 'block_size': 27 }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config.get('block_size'), result[2].get('block_size'))

    def test_page_size_passes_through(self):
        test_config = { 'page_size': 27 }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config.get('page_size'), result[2].get('page_size'))

    def test_heartbeat_frequency_passes_through(self):
        test_config = { 'heartbeat_frequency': 27 }
        result = config.parse.apply_defaults_in_context(test_config, *self.minimal_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext())
        self.assertEqual(test_config.get('heartbeat_frequency'), result[2].get('heartbeat_frequency'))

class FoundMoreContext:
    def find(self, module):
        return {'name': module, 'fname': 'xxyzzy/'+module, '_is_instruction_prefetcher': module.endswith('_instr')}

    def find_all(self):
        return [{'name': 'extra', 'fname': 'aaaabbbb/extra', '_is_instruction_prefetcher': False}]

class CompileAllModulesTests(unittest.TestCase):

    def test_no_compile_all_finds_given_branch(self):
        result = config.parse.parse_config_in_context({ 'branch_predictor': 'test_branch' }, FoundMoreContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('test_branch', result[1])

        result_all = config.parse.parse_config_in_context({ 'branch_predictor': 'test_branch' }, FoundMoreContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), True)
        self.assertIn('test_branch', result_all[1])

    def test_no_compile_all_finds_given_btb(self):
        result = config.parse.parse_config_in_context({ 'btb': 'test_btb' }, PassthroughContext(), FoundMoreContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('test_btb', result[1])

        result_all = config.parse.parse_config_in_context({ 'btb': 'test_btb' }, PassthroughContext(), FoundMoreContext(), PassthroughContext(), PassthroughContext(), True)
        self.assertIn('test_btb', result_all[1])

    def test_no_compile_all_finds_given_pref(self):
        result = config.parse.parse_config_in_context({ 'L1D': { 'prefetcher': 'test_pref' } }, PassthroughContext(), PassthroughContext(), FoundMoreContext(), PassthroughContext(), False)
        self.assertIn('test_pref', result[1])

        result_all = config.parse.parse_config_in_context({ 'L1D': { 'prefetcher': 'test_pref' } }, PassthroughContext(), PassthroughContext(), FoundMoreContext(), PassthroughContext(), True)
        self.assertIn('test_pref', result_all[1])

    def test_no_compile_all_finds_given_repl(self):
        result = config.parse.parse_config_in_context({ 'L1D': { 'prefetcher': 'test_repl' } }, PassthroughContext(), PassthroughContext(), PassthroughContext(), FoundMoreContext(), False)
        self.assertIn('test_repl', result[1])

        result_all = config.parse.parse_config_in_context({ 'L1D': { 'prefetcher': 'test_repl' } }, PassthroughContext(), PassthroughContext(), PassthroughContext(), FoundMoreContext(), True)
        self.assertIn('test_repl', result_all[1])

    def test_compile_all_finds_extra_branch(self):
        result = config.parse.parse_config_in_context({}, FoundMoreContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertNotIn('extra', result[1])

        result_all = config.parse.parse_config_in_context({}, FoundMoreContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), True)
        self.assertIn('extra', result_all[1])

    def test_compile_all_finds_extra_btb(self):
        result = config.parse.parse_config_in_context({}, PassthroughContext(), FoundMoreContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertNotIn('extra', result[1])

        result_all = config.parse.parse_config_in_context({}, PassthroughContext(), FoundMoreContext(), PassthroughContext(), PassthroughContext(), True)
        self.assertIn('extra', result_all[1])

    def test_compile_all_finds_extra_pref(self):
        result = config.parse.parse_config_in_context({}, PassthroughContext(), PassthroughContext(), FoundMoreContext(), PassthroughContext(), False)
        self.assertNotIn('extra', result[1])

        result_all = config.parse.parse_config_in_context({}, PassthroughContext(), PassthroughContext(), FoundMoreContext(), PassthroughContext(), True)
        self.assertIn('extra', result_all[1])

    def test_compile_all_finds_extra_repl(self):
        result = config.parse.parse_config_in_context({}, PassthroughContext(), PassthroughContext(), PassthroughContext(), FoundMoreContext(), False)
        self.assertNotIn('extra', result[1])

        result_all = config.parse.parse_config_in_context({}, PassthroughContext(), PassthroughContext(), PassthroughContext(), FoundMoreContext(), True)
        self.assertIn('extra', result_all[1])

class NormalizeTests(unittest.TestCase):

    def test_pmem_is_read(self):
        test_val = { 'test': True }
        result = config.parse.normalize({'physical_memory': test_val})
        self.assertEqual(result[3], test_val)

    def test_pmem_defaults_to_empty(self):
        result = config.parse.normalize({})
        self.assertEqual(result[3], {})

    def test_vmem_is_read(self):
        test_val = { 'test': True }
        result = config.parse.normalize({'virtual_memory': test_val})
        self.assertEqual(result[4], test_val)

    def test_vmem_defaults_to_empty(self):
        result = config.parse.normalize({})
        self.assertEqual(result[4], {})

    def test_caches_is_read(self):
        test_val = { 'name': '__test__' }
        result = config.parse.normalize({'caches': [test_val]})
        self.assertIn(test_val['name'], result[1].keys())

    def test_ptws_is_read(self):
        test_val = { 'name': '__test__' }
        result = config.parse.normalize({'ptws': [test_val]})
        self.assertIn(test_val['name'], result[2].keys())

    def test_caches_are_moved_from_cores(self):
        test_val = { 'name': '__test__' }
        cache_names = ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB')

        for name in cache_names:
            with self.subTest(name=name):
                result = config.parse.normalize({ 'ooo_cpu': [{ name: test_val }] })
                self.assertIn(test_val['name'], result[1].keys())
                self.assertEqual(test_val['name'], result[0][0][name])

    def test_caches_are_moved_from_config(self):
        test_val = { 'opt': '__test__' }
        cache_names = ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB')

        for name in cache_names:
            with self.subTest(name=name):
                result = config.parse.normalize({ name: test_val })
                self.assertEqual(test_val['opt'], result[1][result[0][0][name]]['opt'])

    def test_cache_take_priority_from_cache_array(self):
        high_priority_val = '__fromarray__'
        cache_names = ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB')

        for name in cache_names:
            with self.subTest(name=name):
                result = config.parse.normalize({
                    name: { 'opt': '__fromconfig__' },
                    'caches': [{ 'name': 'test_'+name, 'opt': '__fromarray__' }],
                    'ooo_cpu': [{ name: { 'name': 'test_'+name, 'opt': '__fromcore__' } }]
                })
                self.assertEqual(high_priority_val, result[1][result[0][0][name]]['opt'])

    def test_caches_can_refer_by_name(self):
        test_val = { 'name': '__test__' }
        cache_names = ('L1I', 'L1D', 'L2C', 'ITLB', 'DTLB', 'STLB')

        for name in cache_names:
            with self.subTest(name=name):
                result = config.parse.normalize({ 'ooo_cpu': [{ name: test_val['name'] }], 'caches': [test_val] })
                self.assertIn(test_val['name'], result[1].keys())
                self.assertEqual(test_val['name'], result[0][0][name])

    def test_ptws_are_moved_from_cores(self):
        test_val = { 'name': '__test__' }
        result = config.parse.normalize({ 'ooo_cpu': [{ 'PTW': test_val }] })
        self.assertIn(test_val['name'], result[2].keys())
        self.assertEqual(test_val['name'], result[0][0]['PTW'])

    def test_ptws_are_moved_from_config(self):
        test_val = { 'opt': '__test__' }
        result = config.parse.normalize({ 'PTW': test_val })
        self.assertEqual(test_val['opt'], result[2][result[0][0]['PTW']]['opt'])

    def test_ptws_take_priority_from_ptw_array(self):
        high_priority_val = '__fromarray__'

        result = config.parse.normalize({
            'PTW': { 'opt': '__fromconfig__' },
            'caches': [{ 'name': 'test_ptw', 'opt': '__fromarray__' }],
            'ooo_cpu': [{ 'PTW': { 'name': 'test_ptw', 'opt': '__fromcore__' } }]
        })
        self.assertEqual(high_priority_val, result[1][result[0][0]['PTW']]['opt'])

    def test_ptws_can_refer_by_name(self):
        test_val = { 'name': '__test__' }
        result = config.parse.normalize({ 'ooo_cpu': [{ 'PTW': test_val['name'] }], 'ptws': [test_val] })
        self.assertIn(test_val['name'], result[2].keys())
        self.assertEqual(test_val['name'], result[0][0]['PTW'])

    def test_core_keys_are_copied(self):
        keys = ('frequency', 'ifetch_buffer_size', 'decode_buffer_size', 'dispatch_buffer_size', 'rob_size', 'lq_size', 'sq_size', 'fetch_width', 'decode_width', 'dispatch_width', 'execute_width', 'lq_width', 'sq_width', 'retire_width', 'mispredict_penalty', 'scheduler_size', 'decode_latency', 'dispatch_latency', 'schedule_latency', 'execute_latency', 'branch_predictor', 'btb', 'DIB')
        for k in keys:
            with self.subTest(key=k):
                result = config.parse.normalize({ k: 2016 })
                self.assertEqual(2016, result[0][0].get(k))
