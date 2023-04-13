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

class HomogeneousCoreParseTests(unittest.TestCase):

    def setUp(self):
        self.configs = (

                # Default
                {},

                # Single core in root
                {
                    'frequency': 2,
                    'ifetch_buffer_size': 2,
                    'decode_buffer_size': 2,
                    'dispatch_buffer_size': 2,
                    'rob_size': 2,
                    'lq_size': 2,
                    'sq_size': 2,
                    'fetch_width': 2,
                    'decode_width': 2,
                    'dispatch_width': 2,
                    'execute_width': 2,
                    'lq_width': 2,
                    'sq_width': 2,
                    'retire_width': 2,
                    'mispredict_penalty': 2,
                    'scheduler_size': 2,
                    'decode_latency': 2,
                    'dispatch_latency': 2,
                    'schedule_latency': 2,
                    'execute_latency': 2
                },

                # Single core in list
                {
                    'ooo_cpu': [
                        {
                        'frequency': 2,
                        'ifetch_buffer_size': 2,
                        'decode_buffer_size': 2,
                        'dispatch_buffer_size': 2,
                        'rob_size': 2,
                        'lq_size': 2,
                        'sq_size': 2,
                        'fetch_width': 2,
                        'decode_width': 2,
                        'dispatch_width': 2,
                        'execute_width': 2,
                        'lq_width': 2,
                        'sq_width': 2,
                        'retire_width': 2,
                        'mispredict_penalty': 2,
                        'scheduler_size': 2,
                        'decode_latency': 2,
                        'dispatch_latency': 2,
                        'schedule_latency': 2,
                        'execute_latency': 2
                        }
                    ]
                },

                # Multicore by specification
                { 'num_cores': 2 },

                # Multicore by specification in root
                {
                    'num_cores': 4,
                    'frequency': 2,
                    'ifetch_buffer_size': 2,
                    'decode_buffer_size': 2,
                    'dispatch_buffer_size': 2,
                    'rob_size': 2,
                    'lq_size': 2,
                    'sq_size': 2,
                    'fetch_width': 2,
                    'decode_width': 2,
                    'dispatch_width': 2,
                    'execute_width': 2,
                    'lq_width': 2,
                    'sq_width': 2,
                    'retire_width': 2,
                    'mispredict_penalty': 2,
                    'scheduler_size': 2,
                    'decode_latency': 2,
                    'dispatch_latency': 2,
                    'schedule_latency': 2,
                    'execute_latency': 2
                },

                # Multicore by specification in list
                {
                    'num_cores': 4,
                    'ooo_cpu': [
                        {
                            'frequency': 2,
                            'ifetch_buffer_size': 2,
                            'decode_buffer_size': 2,
                            'dispatch_buffer_size': 2,
                            'rob_size': 2,
                            'lq_size': 2,
                            'sq_size': 2,
                            'fetch_width': 2,
                            'decode_width': 2,
                            'dispatch_width': 2,
                            'execute_width': 2,
                            'lq_width': 2,
                            'sq_width': 2,
                            'retire_width': 2,
                            'mispredict_penalty': 2,
                            'scheduler_size': 2,
                            'decode_latency': 2,
                            'dispatch_latency': 2,
                            'schedule_latency': 2,
                            'execute_latency': 2
                        }
                    ]
                },

                # Specify L1I in root
                {
                    'L1I': {
                        'sets': 2,
                        'ways': 1,
                        'rq_size': 7,
                        'wq_size': 7,
                        'pq_size': 7,
                        'ptwq_size': 7,
                        'mshr_size': 7,
                        'fill_latency': 7,
                        'hit_latency': 7,
                        'max_tag_check': 7,
                        'max_fill': 7,
                        'prefetch_as_load': True,
                        'prefetch_activate': 'WRITE',
                        'prefetcher': 'test_instr'
                    }
                },
                {
                    'L1I': {
                        'prefetch_as_load': False,
                        'prefetcher': 'test' # PassthroughContext identifies this as not an instruction prefetcher
                    }
                },

                # Specify L1I in list
                {
                    'ooo_cpu': [
                        {
                            'L1I': {
                                'sets': 2,
                                'ways': 1,
                                'rq_size': 7,
                                'wq_size': 7,
                                'pq_size': 7,
                                'ptwq_size': 7,
                                'mshr_size': 7,
                                'fill_latency': 7,
                                'hit_latency': 7,
                                'max_tag_check': 7,
                                'max_fill': 7,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'WRITE'
                            }
                        }
                    ]
                },

                # Specify L1D in root
                {
                    'L1D': {
                        'sets': 2,
                        'ways': 1,
                        'rq_size': 7,
                        'wq_size': 7,
                        'pq_size': 7,
                        'ptwq_size': 7,
                        'mshr_size': 7,
                        'fill_latency': 7,
                        'hit_latency': 7,
                        'max_tag_check': 7,
                        'max_fill': 7,
                        'prefetch_as_load': True,
                        'prefetch_activate': 'WRITE'
                    }
                },
                {
                    'L1D': {
                        'prefetch_as_load': False
                    }
                },

                # Specify L1D in list
                {
                    'ooo_cpu': [
                        {
                            'L1D': {
                                'sets': 2,
                                'ways': 1,
                                'rq_size': 7,
                                'wq_size': 7,
                                'pq_size': 7,
                                'ptwq_size': 7,
                                'mshr_size': 7,
                                'fill_latency': 7,
                                'hit_latency': 7,
                                'max_tag_check': 7,
                                'max_fill': 7,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'WRITE'
                            }
                        }
                    ]
                },

                # Specify ITLB in root
                {
                    'ITLB': {
                        'sets': 2,
                        'ways': 1,
                        'rq_size': 7,
                        'wq_size': 7,
                        'pq_size': 7,
                        'ptwq_size': 7,
                        'mshr_size': 7,
                        'fill_latency': 7,
                        'hit_latency': 7,
                        'max_tag_check': 7,
                        'max_fill': 7,
                        'prefetch_as_load': True,
                        'prefetch_activate': 'WRITE'
                    }
                },
                {
                    'ITLB': {
                        'prefetch_as_load': False
                    }
                },

                # Specify ITLB in list
                {
                    'ooo_cpu': [
                        {
                            'ITLB': {
                                'sets': 2,
                                'ways': 1,
                                'rq_size': 7,
                                'wq_size': 7,
                                'pq_size': 7,
                                'ptwq_size': 7,
                                'mshr_size': 7,
                                'fill_latency': 7,
                                'hit_latency': 7,
                                'max_tag_check': 7,
                                'max_fill': 7,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'WRITE'
                            }
                        }
                    ]
                },

                # Specify DTLB in root
                {
                    'DTLB': {
                        'sets': 2,
                        'ways': 1,
                        'rq_size': 7,
                        'wq_size': 7,
                        'pq_size': 7,
                        'ptwq_size': 7,
                        'mshr_size': 7,
                        'fill_latency': 7,
                        'hit_latency': 7,
                        'max_tag_check': 7,
                        'max_fill': 7,
                        'prefetch_as_load': True,
                        'prefetch_activate': 'WRITE'
                    }
                },
                {
                    'DTLB': {
                        'prefetch_as_load': False
                    }
                },

                # Specify DTLB in list
                {
                    'ooo_cpu': [
                        {
                            'DTLB': {
                                'sets': 2,
                                'ways': 1,
                                'rq_size': 7,
                                'wq_size': 7,
                                'pq_size': 7,
                                'ptwq_size': 7,
                                'mshr_size': 7,
                                'fill_latency': 7,
                                'hit_latency': 7,
                                'max_tag_check': 7,
                                'max_fill': 7,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'WRITE'
                            }
                        }
                    ]
                },
            ) # TODO add more

        self.core_keys_to_check = ( 'frequency', 'ifetch_buffer_size', 'decode_buffer_size', 'dispatch_buffer_size', 'rob_size', 'lq_size', 'sq_size', 'fetch_width', 'decode_width', 'dispatch_width', 'execute_width', 'lq_width', 'sq_width', 'retire_width', 'mispredict_penalty', 'scheduler_size', 'decode_latency', 'dispatch_latency', 'schedule_latency', 'execute_latency')
        self.cache_keys_to_check = ( 'sets', 'ways', 'rq_size', 'wq_size', 'pq_size', 'ptwq_size', 'mshr_size', 'hit_latency', 'fill_latency', 'max_tag_check', 'max_fill', 'prefetch_as_load', 'virtual_prefetch', 'wq_check_full_addr', 'prefetch_activate')

        self.branch_context = PassthroughContext()
        self.btb_context = PassthroughContext()
        self.prefetcher_context = PassthroughContext()
        self.replacement_context = PassthroughContext()

    def test_cores_have_names(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cores = result[0]['cores']

                # Ensure each core has a name
                for core in cores:
                    self.assertIn('name', core)

                # Ensure each core has a unique name
                for i in range(len(cores)):
                    self.assertNotIn(cores[i]['name'], [cores[j]['name'] for j in range(len(cores)) if j != i])

    def test_cores_are_homogeneous(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cores = result[0]['cores']

                for key in self.core_keys_to_check:
                    self.assertEqual([core[key] for core in cores], [cores[0][key]]*len(cores))

    def test_caches_have_names(self):
        for c in self.configs:
            for key in ('L1I', 'L1D', 'ITLB', 'DTLB'):
                with self.subTest(config=c, cache_name=key):
                    result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                    cache_names = [core[key] for core in result[0]['cores']]
                    caches = result[0]['caches']

                    # Ensure each core has a name
                    for name in cache_names:
                        self.assertIn(name, [cache['name'] for cache in caches])

                    # Ensure each core has a unique name
                    for i in range(len(cache_names)):
                        self.assertNotIn(cache_names[i], [cache_names[j] for j in range(len(cache_names)) if j != i])

    def test_caches_are_homogeneous(self):
        for c in self.configs:
            for name in ('L1I', 'L1D', 'ITLB', 'DTLB'):
                with self.subTest(config=c, cache_name=name):
                    result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                    cache_names = [core[name] for core in result[0]['cores']]
                    caches = result[0]['caches']

                    for key in self.cache_keys_to_check:
                        first_of = caches[[cache['name'] for cache in caches].index(cache_names[0])][key]
                        self.assertEqual([cache[key] for cache in caches if cache['name'] in cache_names], [first_of]*len(cache_names))

    def test_instruction_caches_have_instruction_prefetchers(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']]
                caches = result[0]['caches']

                instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in instruction_caches:
                    for data in cache['_prefetcher_data']:
                        self.assertTrue(data['_is_instruction_prefetcher'])

    def test_instruction_caches_prefetch_virtually(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']]
                caches = result[0]['caches']

                instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in instruction_caches:
                    self.assertTrue(cache['virtual_prefetch'])

    def test_instruction_and_data_caches_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']] + [core['L1D'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in filtered_caches:
                    self.assertTrue(cache['_needs_translate'])

    def test_tlbs_do_not_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['ITLB'] for core in result[0]['cores']] + [core['DTLB'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in filtered_caches:
                    self.assertFalse(cache['_needs_translate'])

    def test_caches_inherit_core_frequency(self):
        for c in self.configs:
            for name in ('L1I', 'L1D', 'ITLB', 'DTLB'):
                with self.subTest(config=c, cache_name=name):
                    result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                    cache_names_and_frequencies = [(core[name], core['frequency']) for core in result[0]['cores']]
                    caches = result[0]['caches']

                    for cache_name, frequency in cache_names_and_frequencies:
                        cache_freq = caches[[cache['name'] for cache in caches].index(cache_name)]['frequency']
                        self.assertEqual(frequency, cache_freq)

class HeterogeneousCoreDuplicationParseTests(unittest.TestCase):

    def setUp(self):
        self.configs = (
                {
                    'ooo_cpu': [
                        {
                            'frequency': 2,
                            'ifetch_buffer_size': 2,
                            'decode_buffer_size': 2,
                            'dispatch_buffer_size': 2,
                            'rob_size': 2,
                            'lq_size': 2,
                            'sq_size': 2,
                            'fetch_width': 2,
                            'decode_width': 2,
                            'dispatch_width': 2,
                            'execute_width': 2,
                            'lq_width': 2,
                            'sq_width': 2,
                            'retire_width': 2,
                            'mispredict_penalty': 2,
                            'scheduler_size': 2,
                            'decode_latency': 2,
                            'dispatch_latency': 2,
                            'schedule_latency': 2,
                            'execute_latency': 2
                        },
                        {
                            'frequency': 3,
                            'ifetch_buffer_size': 3,
                            'decode_buffer_size': 3,
                            'dispatch_buffer_size': 3,
                            'rob_size': 3,
                            'lq_size': 3,
                            'sq_size': 3,
                            'fetch_width': 3,
                            'decode_width': 3,
                            'dispatch_width': 3,
                            'execute_width': 3,
                            'lq_width': 3,
                            'sq_width': 3,
                            'retire_width': 3,
                            'mispredict_penalty': 3,
                            'scheduler_size': 3,
                            'decode_latency': 3,
                            'dispatch_latency': 3,
                            'schedule_latency': 3,
                            'execute_latency': 3
                        }
                    ]
                },
                {
                    'ooo_cpu': [
                        {
                            'L1D': {
                                'sets': 2,
                                'ways': 1,
                                'rq_size': 7,
                                'wq_size': 7,
                                'pq_size': 7,
                                'ptwq_size': 7,
                                'mshr_size': 7,
                                'fill_latency': 7,
                                'hit_latency': 7,
                                'max_tag_check': 7,
                                'max_fill': 7,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'WRITE'
                            }
                        },
                        {
                            'L1D': {
                                'sets': 3,
                                'ways': 2,
                                'rq_size': 8,
                                'wq_size': 8,
                                'pq_size': 8,
                                'ptwq_size': 8,
                                'mshr_size': 8,
                                'fill_latency': 8,
                                'hit_latency': 8,
                                'max_tag_check': 8,
                                'max_fill': 8,
                                'prefetch_as_load': True,
                                'prefetch_activate': 'LOAD'
                            }
                        }
                    ]
                },
                {
                    'caches': [
                        {
                            'name': 'test_a',
                            'sets': 2,
                            'ways': 1,
                            'rq_size': 7,
                            'wq_size': 7,
                            'pq_size': 7,
                            'ptwq_size': 7,
                            'mshr_size': 7,
                            'fill_latency': 7,
                            'hit_latency': 7,
                            'max_tag_check': 7,
                            'max_fill': 7,
                            'prefetch_as_load': True,
                            'prefetch_activate': 'WRITE'
                        },
                        {
                            'name': 'test_b',
                            'sets': 3,
                            'ways': 2,
                            'rq_size': 8,
                            'wq_size': 8,
                            'pq_size': 8,
                            'ptwq_size': 8,
                            'mshr_size': 8,
                            'fill_latency': 8,
                            'hit_latency': 8,
                            'max_tag_check': 8,
                            'max_fill': 8,
                            'prefetch_as_load': True,
                            'prefetch_activate': 'LOAD'
                        }
                    ],
                    'ooo_cpu': [
                        {
                            'L1I': 'test_a'
                        },
                        {
                            'L1I': 'test_b'
                        }
                    ]
                },
                {
                    'caches': [
                        {
                            'name': 'test_a',
                            'sets': 2,
                            'ways': 1,
                            'rq_size': 7,
                            'wq_size': 7,
                            'pq_size': 7,
                            'ptwq_size': 7,
                            'mshr_size': 7,
                            'fill_latency': 7,
                            'hit_latency': 7,
                            'max_tag_check': 7,
                            'max_fill': 7,
                            'prefetch_as_load': True,
                            'prefetch_activate': 'WRITE'
                        },
                        {
                            'name': 'test_b',
                            'sets': 3,
                            'ways': 2,
                            'rq_size': 8,
                            'wq_size': 8,
                            'pq_size': 8,
                            'ptwq_size': 8,
                            'mshr_size': 8,
                            'fill_latency': 8,
                            'hit_latency': 8,
                            'max_tag_check': 8,
                            'max_fill': 8,
                            'prefetch_as_load': True,
                            'prefetch_activate': 'LOAD'
                        }
                    ],
                    'ooo_cpu': [
                        {
                            'L1D': 'test_a'
                        },
                        {
                            'L1D': 'test_b'
                        }
                    ]
                }
            )

        self.core_keys_to_check = ( 'frequency', 'ifetch_buffer_size', 'decode_buffer_size', 'dispatch_buffer_size', 'rob_size', 'lq_size', 'sq_size', 'fetch_width', 'decode_width', 'dispatch_width', 'execute_width', 'lq_width', 'sq_width', 'retire_width', 'mispredict_penalty', 'scheduler_size', 'decode_latency', 'dispatch_latency', 'schedule_latency', 'execute_latency')
        self.cache_keys_to_check = ( 'sets', 'ways', 'rq_size', 'wq_size', 'pq_size', 'ptwq_size', 'mshr_size', 'hit_latency', 'fill_latency', 'max_tag_check', 'max_fill', 'prefetch_as_load', 'virtual_prefetch', 'wq_check_full_addr', 'prefetch_activate')

        self.branch_context = PassthroughContext()
        self.btb_context = PassthroughContext()
        self.prefetcher_context = PassthroughContext()
        self.replacement_context = PassthroughContext()

    def test_cores_have_names(self):
        for n,c in itertools.product((2,3,4,8), self.configs):
            with self.subTest(config=c, count=n):
                result = config.parse.parse_config_in_context({'num_cores':n, **c}, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cores = result[0]['cores']

                # Ensure each core has a name
                for core in cores:
                    self.assertIn('name', core)

                # Ensure each core has a unique name
                for i in range(len(cores)):
                    self.assertNotIn(cores[i]['name'], [cores[j]['name'] for j in range(len(cores)) if j != i])

    def test_caches_have_names(self):
        for n,c,key in itertools.product((2,3,4,8), self.configs, ('L1I', 'L1D', 'ITLB', 'DTLB')):
            with self.subTest(config=c, count=n, cache_name=key):
                result = config.parse.parse_config_in_context({'num_cores':n, **c}, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = list(set(core[key] for core in result[0]['cores']))
                caches = result[0]['caches']

                # Ensure each core has a name
                for name in cache_names:
                    self.assertIn(name, [cache['name'] for cache in caches])

                # Ensure each core has a unique name
                for i in range(len(cache_names)):
                    self.assertNotIn(cache_names[i], [cache_names[j] for j in range(len(cache_names)) if j != i])

    def test_instruction_caches_have_instruction_prefetchers(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']]
                caches = result[0]['caches']

                instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in instruction_caches:
                    for data in cache['_prefetcher_data']:
                        self.assertTrue(data['_is_instruction_prefetcher'])

    def test_instruction_caches_prefetch_virtually(self):
        for n,c in itertools.product((2,3,4,8), self.configs):
            with self.subTest(config=c, count=n):
                result = config.parse.parse_config_in_context({'num_cores':n, **c}, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']]
                caches = result[0]['caches']

                instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in instruction_caches:
                    self.assertTrue(cache['virtual_prefetch'])

    def test_instruction_and_data_caches_need_translation(self):
        for n,c in itertools.product((2,3,4,8), self.configs):
            with self.subTest(config=c, count=n):
                result = config.parse.parse_config_in_context({'num_cores':n, **c}, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']] + [core['L1D'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in filtered_caches:
                    self.assertTrue(cache['_needs_translate'])

    def test_tlbs_do_not_need_translation(self):
        for n,c in itertools.product((2,3,4,8), self.configs):
            with self.subTest(config=c, count=n):
                result = config.parse.parse_config_in_context({'num_cores':n, **c}, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['ITLB'] for core in result[0]['cores']] + [core['DTLB'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]

                for cache in filtered_caches:
                    self.assertFalse(cache['_needs_translate'])

class EnvironmentParseTests(unittest.TestCase):

    def test_cc_passes_through(self):
        test_config = { 'CC': 'cc' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

    def test_cxx_passes_through(self):
        test_config = { 'CXX': 'cxx' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

    def test_cppflags_passes_through(self):
        test_config = { 'CPPFLAGS': 'cppflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

    def test_cxxflags_passes_through(self):
        test_config = { 'CXXFLAGS': 'cxxflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

    def test_ldflags_passes_through(self):
        test_config = { 'LDFLAGS': 'ldflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

    def test_ldlibs_passes_through(self):
        test_config = { 'LDLIBS': 'ldlibs' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[3])

class ConfigRootPassthroughParseTests(unittest.TestCase):

    def test_block_size_passes_through(self):
        test_config = { 'block_size': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('block_size', result[2])
        self.assertEqual(test_config.get('block_size'), result[2].get('block_size'))

    def test_page_size_passes_through(self):
        test_config = { 'page_size': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('page_size', result[2])
        self.assertEqual(test_config.get('page_size'), result[2].get('page_size'))

    def test_heartbeat_frequency_passes_through(self):
        test_config = { 'heartbeat_frequency': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('heartbeat_frequency', result[2])
        self.assertEqual(test_config.get('heartbeat_frequency'), result[2].get('heartbeat_frequency'))


