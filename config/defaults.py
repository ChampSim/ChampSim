#    Copyright 2023 The ChampSim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import math

from . import util

default_l1i  = {
        'sets': 64,
        'ways': 8,
        'rq_size': 64,
        'wq_size': 64,
        'pq_size': 32,
        'ptwq_size': 0,
        'mshr_size': 8,
        'latency': 4,
        'fill_latency': 1,
        'max_tag_check': 2,
        'max_fill': 2,
        'prefetch_as_load': False,
        'virtual_prefetch': True,
        'wq_check_full_addr': True,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no_instr',
        'replacement': 'lru'
        }

default_l1d  = {
        'sets': 64,
        'ways': 12,
        'rq_size': 64,
        'wq_size': 64,
        'pq_size': 8,
        'ptwq_size': 5,
        'mshr_size': 16,
        'latency': 5,
        'fill_latency': 1,
        'max_tag_check': 2,
        'max_fill': 2,
        'prefetch_as_load': False,
        'virtual_prefetch': False,
        'wq_check_full_addr': True,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru'
        }

default_l2c  = {
        'sets': 1024,
        'ways': 8,
        'rq_size': 32,
        'wq_size': 32,
        'pq_size': 16,
        'ptwq_size': 5,
        'mshr_size': 32,
        'latency': 10,
        'fill_latency': 1,
        'max_tag_check': 1,
        'max_fill': 1,
        'prefetch_as_load': False,
        'virtual_prefetch': False,
        'wq_check_full_addr': False,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru'
        }

default_itlb = {
        'sets': 16,
        'ways': 4,
        'rq_size': 16,
        'wq_size': 16,
        'pq_size': 0,
        'ptwq_size': 0,
        'mshr_size': 8,
        'latency': 1,
        'fill_latency': 1,
        'max_tag_check': 2,
        'max_fill': 2,
        'prefetch_as_load': False,
        'virtual_prefetch': True,
        'wq_check_full_addr': True,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru'
        }

default_dtlb = {
        'sets': 16,
        'ways': 4,
        'rq_size': 16,
        'wq_size': 16,
        'pq_size': 0,
        'ptwq_size': 0,
        'mshr_size': 8,
        'latency': 1,
        'fill_latency': 1,
        'max_tag_check': 2,
        'max_fill': 2,
        'prefetch_as_load': False,
        'virtual_prefetch': False,
        'wq_check_full_addr': True,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru'
        }

default_stlb = {
        'sets': 128,
        'ways': 12,
        'rq_size': 32,
        'wq_size': 32,
        'pq_size': 0,
        'ptwq_size': 0,
        'mshr_size': 16,
        'latency': 8,
        'fill_latency': 1,
        'max_tag_check': 1,
        'max_fill': 1,
        'prefetch_as_load': False,
        'virtual_prefetch': False,
        'wq_check_full_addr': False,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru'
        }

default_llc  = {
        'latency': 20,
        'fill_latency': 1,
        'prefetch_as_load': False,
        'virtual_prefetch': False,
        'wq_check_full_addr': False,
        'prefetch_activate': 'LOAD,PREFETCH',
        'prefetcher': 'no',
        'replacement': 'lru',
        'lower_level': 'DRAM'
        }

default_ptw = {
        'pscl5_set' : 1,
        'pscl5_way' : 2,
        'pscl4_set' : 1,
        'pscl4_way': 4,
        'pscl3_set' : 2,
        'pscl3_way' : 4,
        'pscl2_set' : 4,
        'pscl2_way': 8,
        'ptw_rq_size': 16,
        'ptw_mshr_size': 5,
        'ptw_max_read': 2,
        'ptw_max_write': 2
        }

# Defaults for first-level caches
def named_l1i_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'L1I'), 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'L2C'), 'lower_translate': util.read_element_name(cpu, 'ITLB'), '_needs_translate': True, '_is_instruction_cache': True, **default_l1i}

def named_l1d_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'L1D'), 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'L2C'), 'lower_translate': util.read_element_name(cpu, 'DTLB'), '_needs_translate': True, **default_l1d}

def named_itlb_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'ITLB'), 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'STLB'), **default_itlb}

def named_dtlb_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'DTLB'), 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'STLB'), **default_dtlb}

# Defaults for second-level caches
def named_l2c_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'L2C'), 'frequency': cpu['frequency'], 'lower_level': 'LLC', 'lower_translate': util.read_element_name(cpu, 'STLB'), **default_l2c}

def sequence_l2c_defaults(name, uls):
    uls = list(uls)
    intern_default_l2c  = { 'latency': 10, 'fill_latency': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
    return {'name': name, 'frequency': max(x['frequency'] for x in uls), 'sets': 512*len(uls), 'ways': 8, 'rq_size': 16*len(uls), 'wq_size': 16*len(uls), 'pq_size': 16*len(uls), 'mshr_size': 32*len(uls), 'max_tag_check': math.ceil(0.5*len(uls)), 'max_fill': math.ceil(0.5*len(uls)), **intern_default_l2c}

def named_stlb_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'STLB'), 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'PTW'), **default_stlb}

def sequence_stlb_defaults(name, uls):
    uls = list(uls)
    intern_default_stlb  = { 'latency': 8, 'fill_latency': 1, 'prefetch_as_load': False, 'virtual_prefetch': False, 'wq_check_full_addr': False, 'prefetch_activate': 'LOAD,PREFETCH', 'prefetcher': 'no', 'replacement': 'lru'}
    return {'name': name, 'frequency': max(x['frequency'] for x in uls), 'sets': 64*len(uls), 'ways': 12, 'rq_size': 16*len(uls), 'wq_size': 16*len(uls), 'pq_size': 16*len(uls), 'mshr_size': 8*len(uls), 'max_tag_check': math.ceil(0.5*len(uls)), 'max_fill': math.ceil(0.5*len(uls)), **intern_default_stlb}

# Defaults for third-level caches
def named_ptw_defaults(cpu):
    return {'name': util.read_element_name(cpu, 'PTW'), 'cpu': cpu['index'], 'frequency': cpu['frequency'], 'lower_level': util.read_element_name(cpu, 'L1D'), **default_ptw}

def named_llc_defaults(name, uls):
    uls = list(uls)
    return {'name': name, 'frequency': max(x['frequency'] for x in uls), 'sets': 2048*len(uls), 'ways': 16, 'rq_size': 32*len(uls), 'wq_size': 32*len(uls), 'pq_size': 32*len(uls), 'ptwq_size': 5*len(uls), 'mshr_size': 64*len(uls), 'max_tag_check': len(uls), 'max_fill': len(uls), **default_llc}

