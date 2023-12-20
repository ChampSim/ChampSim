import unittest
import itertools
import tempfile
import os

import config.instantiation_file

class VectorStringTest(unittest.TestCase):

    def test_empty_list(self):
        self.assertEqual(config.instantiation_file.vector_string([]), '{}');

    def test_list_with_one(self):
        self.assertEqual(config.instantiation_file.vector_string(['a']), 'a');

    def test_list_with_two(self):
        self.assertEqual(config.instantiation_file.vector_string(['a','b']), '{a, b}');

class CpuBuilderTest(unittest.TestCase):

    def get_element_diff(self, added_lines, **kwargs):
        base_cpu = { 'name': 'test_cpu' }
        empty = list(config.instantiation_file.get_cpu_builder(base_cpu))
        modified = list(config.instantiation_file.get_cpu_builder({**base_cpu, **kwargs}))
        self.assertEqual({l.strip() for l in itertools.chain(empty, added_lines)}, {l.strip() for l in modified}) # Ignore whitespace

    def test_ifetch_buffer_size(self):
        self.get_element_diff(['.ifetch_buffer_size(1)'], ifetch_buffer_size=1)

    def test_decode_buffer_size(self):
        self.get_element_diff(['.decode_buffer_size(1)'], decode_buffer_size=1)

    def test_dispatch_buffer_size(self):
        self.get_element_diff(['.dispatch_buffer_size(1)'], dispatch_buffer_size=1)

    def test_rob_size(self):
        self.get_element_diff(['.rob_size(1)'], rob_size=1)

    def test_lq_size(self):
        self.get_element_diff(['.lq_size(1)'], lq_size=1)

    def test_sq_size(self):
        self.get_element_diff(['.sq_size(1)'], sq_size=1)

    def test_fetch_width(self):
        self.get_element_diff(['.fetch_width(champsim::bandwidth::maximum_type{1})'], fetch_width=1)

    def test_decode_width(self):
        self.get_element_diff(['.decode_width(champsim::bandwidth::maximum_type{1})'], decode_width=1)

    def test_dispatch_width(self):
        self.get_element_diff(['.dispatch_width(champsim::bandwidth::maximum_type{1})'], dispatch_width=1)

    def test_scheduler_size(self):
        self.get_element_diff(['.schedule_width(champsim::bandwidth::maximum_type{1})'], scheduler_size=1)

    def test_execute_width(self):
        self.get_element_diff(['.execute_width(champsim::bandwidth::maximum_type{1})'], execute_width=1)

    def test_lq_width(self):
        self.get_element_diff(['.lq_width(champsim::bandwidth::maximum_type{1})'], lq_width=1)

    def test_sq_width(self):
        self.get_element_diff(['.sq_width(champsim::bandwidth::maximum_type{1})'], sq_width=1)

    def test_retire_width(self):
        self.get_element_diff(['.retire_width(champsim::bandwidth::maximum_type{1})'], retire_width=1)

    def test_mispredict_penalty(self):
        self.get_element_diff(['.mispredict_penalty(1)'], mispredict_penalty=1)

    def test_decode_latency(self):
        self.get_element_diff(['.decode_latency(1)'], decode_latency=1)

    def test_dispatch_latency(self):
        self.get_element_diff(['.dispatch_latency(1)'], dispatch_latency=1)

    def test_schedule_latency(self):
        self.get_element_diff(['.schedule_latency(1)'], schedule_latency=1)

    def test_execute_latency(self):
        self.get_element_diff(['.execute_latency(1)'], execute_latency=1)

    def test_dib_set(self):
        self.get_element_diff(['.dib_set(1)'], dib_set=1)

    def test_dib_way(self):
        self.get_element_diff(['.dib_way(1)'], dib_way=1)

    def test_dib_window(self):
        self.get_element_diff(['.dib_window(1)'], dib_window=1)

    def test_dib_set_dict(self):
        self.get_element_diff(['.dib_set(1)'], DIB={ 'sets': 1 })

    def test_dib_way_dict(self):
        self.get_element_diff(['.dib_way(1)'], DIB={ 'ways': 1 })

    def test_dib_window_dict(self):
        self.get_element_diff(['.dib_window(1)'], DIB={ 'window_size': 1 })

    def test_branch_predictor(self):
        self.get_element_diff(['.branch_predictor<class a_class>()'], _branch_predictor_data=[{ 'name': 'a', 'class': 'a_class' }])
        self.get_element_diff(['.branch_predictor<class a_class, class b_class>()'], _branch_predictor_data=[{ 'name': 'a', 'class': 'a_class' }, { 'name': 'b', 'class': 'b_class' }])

    def test_btb(self):
        self.get_element_diff(['.btb<class a_class>()'], _btb_data=[{ 'name': 'a', 'class': 'a_class' }])
        self.get_element_diff(['.btb<class a_class, class b_class>()'], _btb_data=[{ 'name': 'a', 'class': 'a_class' }, { 'name': 'b', 'class': 'b_class' }])

class CacheBuilderTests(unittest.TestCase):

    def get_element_diff(self, added_lines, **kwargs):
        base_cache = { 'name': 'test_cache' }
        upper_levels = { 'test_cache': { 'upper_channels': [] } }
        empty = list(config.instantiation_file.get_cache_builder(base_cache, upper_levels))
        modified = list(config.instantiation_file.get_cache_builder({**base_cache, **kwargs}, upper_levels))
        self.assertEqual({l.strip() for l in itertools.chain(empty, added_lines)}, {l.strip() for l in modified}) # Ignore whitespace

    def test_size(self):
        self.get_element_diff(['.size(1)'], size=1)

    def test_log2_size(self):
        self.get_element_diff(['.log2_size(1)'], log2_size=1)

    def test_sets(self):
        self.get_element_diff(['.sets(1)'], sets=1)

    def test_log2_sets(self):
        self.get_element_diff(['.log2_sets(1)'], log2_sets=1)

    def test_ways(self):
        self.get_element_diff(['.ways(1)'], ways=1)

    def test_log2_ways(self):
        self.get_element_diff(['.log2_ways(1)'], log2_ways=1)

    def test_pq_size(self):
        self.get_element_diff(['.pq_size(1)'], pq_size=1)

    def test_mshr_size(self):
        self.get_element_diff(['.mshr_size(1)'], mshr_size=1)

    def test_latency(self):
        self.get_element_diff(['.latency(1)'], latency=1)

    def test_hit_latency(self):
        self.get_element_diff(['.hit_latency(1)'], hit_latency=1)

    def test_fill_latency(self):
        self.get_element_diff(['.fill_latency(1)'], fill_latency=1)

    def test_max_tag_check(self):
        self.get_element_diff(['.tag_bandwidth(champsim::bandwidth::maximum_type{1})'], max_tag_check=1)

    def test_max_fill(self):
        self.get_element_diff(['.fill_bandwidth(champsim::bandwidth::maximum_type{1})'], max_fill=1)

    def test_prefetch_as_load(self):
        self.get_element_diff(['.set_prefetch_as_load()'], prefetch_as_load=True)
        self.get_element_diff(['.reset_prefetch_as_load()'], prefetch_as_load=False)

    def test_wq_check_full_addr(self):
        self.get_element_diff(['.set_wq_checks_full_addr()'], wq_check_full_addr=True)
        self.get_element_diff(['.reset_wq_checks_full_addr()'], wq_check_full_addr=False)

    def test_virtual_prefetch(self):
        self.get_element_diff(['.set_virtual_prefetch()'], virtual_prefetch=True)
        self.get_element_diff(['.reset_virtual_prefetch()'], virtual_prefetch=False)

    def test_prefetch_activate(self):
        self.get_element_diff(['.prefetch_activate(access_type::LOAD)'], prefetch_activate=['LOAD'])
        self.get_element_diff(['.prefetch_activate(access_type::LOAD, access_type::WRITE)'], prefetch_activate=['LOAD', 'WRITE'])

    def test_lower_translate(self):
        self.get_element_diff(['.lower_translate(&test_cache_to_test_lt_channel)'], lower_translate='test_lt')

    def test_prefetcher(self):
        self.get_element_diff(['.prefetcher<class a_class>()'], _prefetcher_data=[{ 'name': 'a', 'class': 'a_class' }])
        self.get_element_diff(['.prefetcher<class a_class, class b_class>()'], _prefetcher_data=[{ 'name': 'a', 'class': 'a_class' }, { 'name': 'b', 'class': 'b_class' }])

    def test_replacement(self):
        self.get_element_diff(['.replacement<class a_class>()'], _replacement_data=[{ 'name': 'a', 'class': 'a_class' }])
        self.get_element_diff(['.replacement<class a_class, class b_class>()'], _replacement_data=[{ 'name': 'a', 'class': 'a_class' }, { 'name': 'b', 'class': 'b_class' }])

class PageTableWalkerBuilderTests(unittest.TestCase):

    def get_element_diff(self, added_lines, **kwargs):
        base_ptw = { 'name': 'test_ptw' }
        upper_levels = { 'test_ptw': { 'upper_channels': [] } }
        empty = list(config.instantiation_file.get_ptw_builder(base_ptw, upper_levels))
        modified = list(config.instantiation_file.get_ptw_builder({**base_ptw, **kwargs}, upper_levels))
        self.assertEqual({l.strip() for l in itertools.chain(empty, added_lines)}, {l.strip() for l in modified}) # Ignore whitespace

    def test_mshr_size(self):
        self.get_element_diff(['.mshr_size(1)'], mshr_size=1)

    def test_max_read(self):
        self.get_element_diff(['.tag_bandwidth(champsim::bandwidth::maximum_type{1})'], max_read=1)

    def test_max_write(self):
        self.get_element_diff(['.fill_bandwidth(champsim::bandwidth::maximum_type{1})'], max_write=1)

    def test_pscl5(self):
        self.get_element_diff(['.add_pscl(5, 1, 2)'], pscl5_set=1, pscl5_way=2)

    def test_pscl4(self):
        self.get_element_diff(['.add_pscl(4, 1, 2)'], pscl4_set=1, pscl4_way=2)

    def test_pscl3(self):
        self.get_element_diff(['.add_pscl(3, 1, 2)'], pscl3_set=1, pscl3_way=2)

    def test_pscl2(self):
        self.get_element_diff(['.add_pscl(2, 1, 2)'], pscl2_set=1, pscl2_way=2)

class UpperChannelCollectorTests(unittest.TestCase):

    def test_single(self):
        value = (('low', 'up'),)
        self.assertEqual({'low': {'upper_channels': ['up_to_low_channel']}}, config.instantiation_file.upper_channel_collector(value))

    def test_multiple(self):
        value = (('low', 'up1'), ('low', 'up2'))
        self.assertEqual({'low': {'upper_channels': ['up1_to_low_channel', 'up2_to_low_channel']}}, config.instantiation_file.upper_channel_collector(value))

class GetUpperLevelsTests(unittest.TestCase):

    def test_empty(self):
        cores = []
        caches = []
        ptws = []
        self.assertEqual([], config.instantiation_file.get_upper_levels(cores, caches, ptws))

    def test_L1Is_are_upper_levels(self):
        cores = [{'L1I': 'test_l1i', 'name': 'test_cpu'}]
        caches = []
        ptws = []
        self.assertEqual([('test_l1i', 'test_cpu')], config.instantiation_file.get_upper_levels(cores, caches, ptws))

    def test_L1Ds_are_upper_levels(self):
        cores = [{'L1D': 'test_l1d', 'name': 'test_cpu'}]
        caches = []
        ptws = []
        self.assertEqual([('test_l1d', 'test_cpu')], config.instantiation_file.get_upper_levels(cores, caches, ptws))

    def test_caches_have_upper_levels(self):
        cores = []
        caches = [{'lower_level': 'test_ll', 'name': 'test_ul'}]
        ptws = []
        self.assertEqual([('test_ll', 'test_ul')], config.instantiation_file.get_upper_levels(cores, caches, ptws))

    def test_ptws_have_upper_levels(self):
        cores = []
        caches = []
        ptws = [{'lower_level': 'test_ll', 'name': 'test_ul'}]
        self.assertEqual([('test_ll', 'test_ul')], config.instantiation_file.get_upper_levels(cores, caches, ptws))

    def test_caches_have_upper_translations(self):
        cores = []
        caches = [{'lower_translate': 'test_ll', 'name': 'test_ul'}]
        ptws = []
        self.assertEqual([('test_ll', 'test_ul')], config.instantiation_file.get_upper_levels(cores, caches, ptws))

class DecorateQueuesTests(unittest.TestCase):
    def test_levels_are_different(self):
        caches = [
            {'name': 'test_l1', 'lower_level': 'test_l2', 'rq_size': 1, 'wq_size': 1, 'pq_size': 1, '_offset_bits': 1, '_queue_check_full_addr': False, '_queue_factor': None},
            {'name': 'test_l2', 'lower_level': 'test_l3', 'rq_size': 2, 'wq_size': 2, 'pq_size': 2, '_offset_bits': 2, '_queue_check_full_addr': False, '_queue_factor': None},
            {'name': 'test_l3', 'lower_level': 'DRAM', 'rq_size': 3, 'wq_size': 3, 'pq_size': 3, '_offset_bits': 3, '_queue_check_full_addr': False, '_queue_factor': None}
        ]
        ptws = []

        evaluated = config.instantiation_file.decorate_queues(caches, ptws, {'name': 'DRAM'})

        self.assertEqual(evaluated.get('test_l2').get('rq_size'), 2)
        self.assertEqual(evaluated.get('test_l2').get('wq_size'), 2)
        self.assertEqual(evaluated.get('test_l2').get('pq_size'), 2)

        self.assertEqual(evaluated.get('test_l3').get('rq_size'), 3)
        self.assertEqual(evaluated.get('test_l3').get('wq_size'), 3)
        self.assertEqual(evaluated.get('test_l3').get('pq_size'), 3)

class GetQueueInfoTests(unittest.TestCase):
    def test_single(self):
        given_uppers = { 'dog': { 'upper_channels': ['cat_to_dog'] } }
        given_decoration = { 'dog': { 'is_good_boy': True } }
        evaluated = config.instantiation_file.get_queue_info(given_uppers, given_decoration)
        expected = [ { 'name': 'cat_to_dog', 'is_good_boy': True } ]
        self.assertEqual(expected, evaluated)

    def test_multiple_uppers(self):
        given_uppers = { 'dog': { 'upper_channels': ['cat_to_dog', 'pig_to_dog'] } }
        given_decoration = { 'dog': { 'is_good_boy': True } }
        evaluated = config.instantiation_file.get_queue_info(given_uppers, given_decoration)
        expected = [ { 'name': 'cat_to_dog', 'is_good_boy': True }, { 'name': 'pig_to_dog', 'is_good_boy': True } ]
        self.assertEqual(expected, evaluated)

    def test_multiple_lowers(self):
        given_uppers = {
            'dog': { 'upper_channels': ['cat_to_dog', 'pig_to_dog'] },
            'cow': { 'upper_channels': ['cat_to_cow', 'pig_to_cow'] }
        }
        given_decoration = {
            'dog': { 'is_good_boy': True },
            'cow': { 'is_good_boy': False }
        }
        evaluated = config.instantiation_file.get_queue_info(given_uppers, given_decoration)
        expected = [
            { 'name': 'cat_to_dog', 'is_good_boy': True },
            { 'name': 'pig_to_dog', 'is_good_boy': True },
            { 'name': 'cat_to_cow', 'is_good_boy': False },
            { 'name': 'pig_to_cow', 'is_good_boy': False }
        ]
        self.assertEqual(expected, evaluated)

class CheckHeaderCompilesForClassTests(unittest.TestCase):
    def test_present(self):
        with tempfile.TemporaryDirectory() as dtemp:
            fname = os.path.join(dtemp, 'test.h')
            with open(fname, 'wt') as wfp:
                print('struct A { explicit A(int*); };', file=wfp)

            self.assertTrue(config.instantiation_file.check_header_compiles_for_class('A', fname))

    def test_absent(self):
        with tempfile.TemporaryDirectory() as dtemp:
            fname = os.path.join(dtemp, 'test.h')
            with open(fname, 'wt') as wfp:
                print('struct A { explicit A(int*); };', file=wfp)

            self.assertFalse(config.instantiation_file.check_header_compiles_for_class('B', fname))
