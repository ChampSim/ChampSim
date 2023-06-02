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

class SplitStringOrListTests(unittest.TestCase):

    def test_empty_string(self):
        self.assertEqual(config.parse.split_string_or_list(''), [])

    def test_empty_list(self):
        self.assertEqual(config.parse.split_string_or_list([]), [])

    def test_list_passthrough(self):
        self.assertEqual(config.parse.split_string_or_list(['a','b']), ['a','b'])

    def test_string_split(self):
        self.assertEqual(config.parse.split_string_or_list('a,b'), ['a','b'])

    def test_string_strip(self):
        self.assertEqual(config.parse.split_string_or_list('a, b'), ['a','b'])

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

    def generate_config(num_cores, core_in_root, l1i_in_root, l1d_in_root, itlb_in_root, dtlb_in_root, core, l1i, l1d, itlb, dtlb):
        generated = { 'num_cores': num_cores }
        if not all((core_in_root, l1i_in_root, l1d_in_root, itlb_in_root, dtlb_in_root)):
            generated['ooo_cpu'] = [{}]

        if core_in_root:
            generated.update(**core)
        else:
            generated['ooo_cpu'][0].update(**core)

        if l1i_in_root:
            generated['L1I'] = l1i
        else:
            generated['ooo_cpu'][0]['L1I'] = l1i

        if l1d_in_root:
            generated['L1D'] = l1d
        else:
            generated['ooo_cpu'][0]['L1D'] = l1d

        if itlb_in_root:
            generated['ITLB'] = itlb
        else:
            generated['ooo_cpu'][0]['ITLB'] = itlb

        if dtlb_in_root:
            generated['DTLB'] = dtlb
        else:
            generated['ooo_cpu'][0]['DTLB'] = dtlb

        return generated

    def setUp(self):
        cores = (
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
            },)

        cache_shapes = (
            {
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
                'prefetch_activate': 'WRITE',
                'prefetch_as_load': True,
                'virtual_prefetch': False,
                'prefetcher': 'test_instr'
            },
            {
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
                'prefetch_activate': 'LOAD',
                'prefetch_as_load': False,
                'virtual_prefetch': True,
                'prefetcher': 'test' # PassthroughContext identifies this as not an instruction prefetcher
            })
            # TODO add more

        self.configs = itertools.starmap(HomogeneousCoreParseTests.generate_config, itertools.product(
            (1,2,4), # num_cores
            (True, False), # core in root
            (True, False), # l1i in root
            (True, False), # l1d in root
            (True, False), # itlb in root
            (True, False), # dtlb in root
            cores,
            cache_shapes,
            cache_shapes,
            cache_shapes,
            cache_shapes
            ))

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
                    self.assertEqual([core.get(key) for core in cores], [cores[0].get(key)]*len(cores))

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
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                for name in ('L1I', 'L1D', 'ITLB', 'DTLB'):
                    cache_names = [core[name] for core in result[0]['cores']]
                    caches = result[0]['caches']

                    for key in self.cache_keys_to_check:
                        first_of = caches[[cache['name'] for cache in caches].index(cache_names[0])].get(key)
                        self.assertEqual([cache.get(key) for cache in caches if cache['name'] in cache_names], [first_of]*len(cache_names))

    def test_instruction_caches_have_instruction_prefetchers(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']]
                caches = result[0]['caches']

                instruction_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
                is_inst_data = {c['name']:[d['_is_instruction_prefetcher'] for d in c['_prefetcher_data']] for c in caches if c['name'] in cache_names}
                self.assertEqual(is_inst_data, {n:[True] for n in cache_names})

    def test_instruction_and_data_caches_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']] + [core['L1D'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
                tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

                self.assertEqual(tlb_names, {c:True for c in tlb_names.keys()})

    def test_tlbs_do_not_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['ITLB'] for core in result[0]['cores']] + [core['DTLB'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
                tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

                self.assertEqual(tlb_names, {c:False for c in tlb_names.keys()})

    def test_caches_inherit_core_frequency(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                for name in ('L1I', 'L1D', 'ITLB', 'DTLB'):
                    cache_names_and_frequencies = [(core[name], core['frequency']) for core in result[0]['cores']]
                    caches = result[0]['caches']

                    for cache_name, frequency in cache_names_and_frequencies:
                        cache_freq = caches[[cache['name'] for cache in caches].index(cache_name)].get('frequency')
                        self.assertEqual(frequency, cache_freq)

class NormalizeConfigTest(unittest.TestCase):

    def test_empty_config_creates_defaults(self):
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config({})
        self.assertEqual(len(cores), 1)
        self.assertIn(cores[0]['PTW'], ptws)
        self.assertEqual(pmem, {})
        self.assertEqual(vmem, {})
        for name in ('L1I', 'L1D', 'ITLB', 'DTLB'):
            self.assertIn(cores[0][name], caches)

    def test_caches_in_root_are_moved_to_cache_array(self):
        for name in ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB'):
            with self.subTest(cache_name=name):
                test_config = {
                        name: {
                            '__test__': True
                        }
                    }
                cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
                self.assertIn(cores[0][name], caches)
                self.assertEqual(caches[cores[0][name]].get('__test__'), True)

    def test_ptws_in_cores_are_moved_to_cache_array(self):
        test_config = {
                'PTW': {
                    '__test__': True
                }
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertIn(cores[0]['PTW'], ptws)
        self.assertEqual(ptws[cores[0]['PTW']].get('__test__'), True)

    def test_caches_in_cores_are_moved_to_cache_array(self):
        for name in ('L1I', 'L1D', 'ITLB', 'DTLB', 'L2C', 'STLB'):
            with self.subTest(cache_name=name):
                test_config = {
                        'ooo_cpu': [{name: {
                            '__test__': True
                        }}]
                    }
                cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
                self.assertIn(cores[0][name], caches)
                self.assertEqual(caches[cores[0][name]].get('__test__'), True)

    def test_ptws_in_cores_are_moved_to_cache_array(self):
        test_config = {
                'ooo_cpu': [{'PTW': {
                    '__test__': True
                }}]
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertIn(cores[0]['PTW'], ptws)
        self.assertEqual(ptws[cores[0]['PTW']].get('__test__'), True)

    def test_cache_array_is_forwarded(self):
        test_config = {
                'caches': [{
                    'name': 'testcache',
                    '__test__': True
                }]
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertIn('testcache', caches)
        self.assertEqual(caches['testcache'].get('__test__'), True)

    def test_ptw_array_is_forwarded(self):
        test_config = {
                'ptws': [{
                    'name': 'testcache',
                    '__test__': True
                }]
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertIn('testcache', ptws)
        self.assertEqual(ptws['testcache'].get('__test__'), True)

    def test_physical_memory_is_forwarded(self):
        test_config = {
                'physical_memory': {
                    '__test__': True
                }
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertEqual(pmem.get('__test__'), True)

    def test_virtual_memory_is_forwarded(self):
        test_config = {
                'virtual_memory': {
                    '__test__': True
                }
            }
        cores, caches, ptws, pmem, vmem = config.parse.normalize_config(test_config)
        self.assertEqual(vmem.get('__test__'), True)

class HeterogeneousCoreDuplicationParseTests(unittest.TestCase):

    def generate_config(num_cores, base, caches):
        generated = { 'num_cores': num_cores, **base }
        generated['ooo_cpu'] = list(config.util.chain(b, c) for b,c in zip(generated['ooo_cpu'], caches))
        return generated

    def setUp(self):
        base_config = {
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
                    ],

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
                    ]
                }
        cache_mods = (
                    [
                        {
                            'L1I': 'test_a'
                        },
                        {
                            'L1I': 'test_b'
                        }
                    ],
                    [
                        {
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
                    ]
            )

        self.configs = itertools.starmap(HeterogeneousCoreDuplicationParseTests.generate_config, itertools.product((2,3,4,8), (base_config,), cache_mods))

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

    def test_caches_have_names(self):
        for c,key in itertools.product(self.configs, ('L1I', 'L1D', 'ITLB', 'DTLB')):
            with self.subTest(config=c, cache_name=key):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
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
                is_inst_data = {c['name']:[d['_is_instruction_prefetcher'] for d in c['_prefetcher_data']] for c in caches if c['name'] in cache_names}
                self.assertEqual(is_inst_data, {n:[True] for n in cache_names})

    def test_instruction_and_data_caches_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['L1I'] for core in result[0]['cores']] + [core['L1D'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
                tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

                self.assertEqual(tlb_names, {c:True for c in tlb_names.keys()})

    def test_tlbs_do_not_need_translation(self):
        for c in self.configs:
            with self.subTest(config=c):
                result = config.parse.parse_config_in_context(c, self.branch_context, self.btb_context, self.prefetcher_context, self.replacement_context, False)
                cache_names = [core['ITLB'] for core in result[0]['cores']] + [core['DTLB'] for core in result[0]['cores']]
                caches = result[0]['caches']

                filtered_caches = [caches[[cache['name'] for cache in caches].index(name)] for name in cache_names]
                tlb_names = {c['name']: ('lower_translate' in c) for c in filtered_caches}

                self.assertEqual(tlb_names, {c:False for c in tlb_names.keys()})

class EnvironmentParseTests(unittest.TestCase):

    def test_cc_passes_through(self):
        test_config = { 'CC': 'cc' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

    def test_cxx_passes_through(self):
        test_config = { 'CXX': 'cxx' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

    def test_cppflags_passes_through(self):
        test_config = { 'CPPFLAGS': 'cppflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

    def test_cxxflags_passes_through(self):
        test_config = { 'CXXFLAGS': 'cxxflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

    def test_ldflags_passes_through(self):
        test_config = { 'LDFLAGS': 'ldflags' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

    def test_ldlibs_passes_through(self):
        test_config = { 'LDLIBS': 'ldlibs' }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertEqual(test_config, result[4])

class ConfigRootPassthroughParseTests(unittest.TestCase):

    def test_block_size_passes_through(self):
        test_config = { 'block_size': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('block_size', result[3])
        self.assertEqual(test_config.get('block_size'), result[3].get('block_size'))

    def test_page_size_passes_through(self):
        test_config = { 'page_size': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('page_size', result[3])
        self.assertEqual(test_config.get('page_size'), result[3].get('page_size'))

    def test_heartbeat_frequency_passes_through(self):
        test_config = { 'heartbeat_frequency': 27 }
        result = config.parse.parse_config_in_context(test_config, PassthroughContext(), PassthroughContext(), PassthroughContext(), PassthroughContext(), False)
        self.assertIn('heartbeat_frequency', result[3])
        self.assertEqual(test_config.get('heartbeat_frequency'), result[3].get('heartbeat_frequency'))

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

