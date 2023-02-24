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

def core_defaults(cpu, name, ll_name=None, lt_name=None):
    retval = {
        'name': util.read_element_name(cpu, name),
        'frequency': cpu['frequency']
    }
    if ll_name is not None:
        retval.update(lower_level=util.read_element_name(cpu, ll_name))
    if lt_name is not None:
        retval.update(lower_translate=util.read_element_name(cpu, lt_name))
    return retval;

def ul_dependent_defaults(*uls, set_factor=512, queue_factor=32, mshr_factor=32, bandwidth_factor=0.5):
    return {
        'frequency': max(x['frequency'] for x in uls),
        'sets': set_factor*len(uls),
        'rq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'wq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'pq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'mshr_size': mshr_factor*len(uls),
        'max_tag_check': math.ceil(bandwidth_factor*len(uls)),
        'max_fill': math.ceil(bandwidth_factor*len(uls))
    }

# Defaults for first-level caches
def named_l1i_defaults(cpu):
    return {
        **core_defaults(cpu, 'L1I', 'L2C', 'ITLB'),
        'rq_size': 64,
        'wq_size': 64,
        'pq_size': 32,
        '_needs_translate': True,
        '_is_instruction_cache': True,
        '_defaults': 'champsim::defaults::default_l1i'
    }

def named_l1d_defaults(cpu):
    return {
        **core_defaults(cpu, 'L1D', 'L2C', 'DTLB'),
        'rq_size': 64,
        'wq_size': 64,
        'pq_size': 8,
        '_needs_translate': True,
        '_defaults': 'champsim::defaults::default_l1d'
    }

def named_itlb_defaults(cpu):
    return {
        **core_defaults(cpu, 'ITLB', 'STLB'),
        'rq_size': 16,
        'wq_size': 16,
        'pq_size': 0,
        '_defaults': 'champsim::defaults::default_itlb'
    }

def named_dtlb_defaults(cpu):
    return {
        **core_defaults(cpu, 'DTLB', 'STLB'),
        'rq_size': 16,
        'wq_size': 16,
        'pq_size': 0,
        '_defaults': 'champsim::defaults::default_dtlb'
    }

# Defaults for second-level caches
def named_l2c_defaults(cpu):
    return { **core_defaults(cpu, 'L2C', lt_name='PTW'), 'lower_level': 'LLC', '_defaults': 'champsim::defaults::default_l2c' }

def sequence_l2c_defaults(name, uls):
    return { 'name': name, **ul_dependent_defaults(*uls, set_factor=512, mshr_factor=32, bandwidth_factor=0.5), '_defaults': 'champsim::defaults::default_l2c' }

def named_stlb_defaults(cpu):
    return { **core_defaults(cpu, 'STLB', 'PTW'), '_defaults': 'champsim::defaults::default_stlb' }

def sequence_stlb_defaults(name, uls):
    return { 'name': name, **ul_dependent_defaults(*uls, set_factor=64, mshr_factor=8, bandwidth_factor=0.5), '_defaults': 'champsim::defaults::default_stlb' }

# Defaults for third-level caches
def named_ptw_defaults(cpu):
    return {
        **core_defaults(cpu, 'PTW', 'L1D'),
        'cpu': cpu['index'],
        'rq_size': 32,
        'wq_size': 32,
        'pq_size': 0
    }

def named_llc_defaults(name, uls):
    return { 'name': name, **ul_dependent_defaults(*uls, set_factor=2048, mshr_factor=64, bandwidth_factor=1), 'lower_level': 'DRAM', '_defaults': 'champsim::defaults::default_llc' }

