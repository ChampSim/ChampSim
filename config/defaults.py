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

import itertools
import math

from . import util

def core_defaults(cpu, name, ll_name=None, lt_name=None):
    retval = {
        'name': util.read_element_name(cpu, name)
    }
    if ll_name is not None:
        retval.update(lower_level=util.read_element_name(cpu, ll_name))
    if lt_name is not None:
        retval.update(lower_translate=util.read_element_name(cpu, lt_name))
    return retval;

def ul_dependent_defaults(*uls, set_factor=512, queue_factor=32, mshr_factor=32, bandwidth_factor=0.5):
    return {
        'sets': set_factor*len(uls),
        'rq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'wq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'pq_size': math.ceil(bandwidth_factor*queue_factor*len(uls)),
        'mshr_size': mshr_factor*len(uls),
        'max_tag_check': math.ceil(bandwidth_factor*len(uls)),
        'max_fill': math.ceil(bandwidth_factor*len(uls))
    }

def defaulter(cores, cache_list, factor_list, key):
    head = lambda n: util.upper_levels_for(cores, n, key=key)
    tail = lambda n: util.upper_levels_for(cache_list, n)

    for ulf, fac in zip(itertools.chain((head,), itertools.repeat(tail)), factor_list):
        yield lambda name: { 'name': name, **ul_dependent_defaults(*ulf(name), **fac) }

def default_path(cores, caches, factor_list, member_list, name):
    for p in (util.iter_system(caches, cpu[name]) for cpu in cores):
        fixed_defaults = itertools.starmap(util.chain, itertools.zip_longest(({'_first_level': True},), member_list, fillvalue={}))
        defaults = defaulter(cores, list(caches.values()), factor_list, name)
        yield from (util.chain(f(c['name']), x) for f,c,x in zip(defaults, p, fixed_defaults))

def l1i_path(cores, caches):
    l1i_factors = (
        { 'set_factor': 64, 'mshr_factor': 32, 'bandwidth_factor': 1 },
        { 'set_factor': 512, 'mshr_factor': 32, 'bandwidth_factor': 0.5 },
        { 'set_factor': 2048, 'mshr_factor': 64, 'bandwidth_factor': 1 }
    )
    l1i_members = (
        { '_is_instruction_cache': True, '_defaults': 'champsim::defaults::default_l1i' },
        { '_defaults': 'champsim::defaults::default_l2c' },
        { '_defaults': 'champsim::defaults::default_llc' }
    )
    p = list(default_path(cores, caches, l1i_factors, l1i_members, 'L1I'))
    yield from p

def l1d_path(cores, caches):
    l1d_factors = (
        { 'set_factor': 64, 'mshr_factor': 32, 'bandwidth_factor': 1 },
        { 'set_factor': 512, 'mshr_factor': 32, 'bandwidth_factor': 0.5 },
        { 'set_factor': 2048, 'mshr_factor': 64, 'bandwidth_factor': 1 }
    )
    l1d_members = (
        { '_defaults': 'champsim::defaults::default_l1d' },
        { '_defaults': 'champsim::defaults::default_l2c' },
        { '_defaults': 'champsim::defaults::default_llc' }
    )
    yield from default_path(cores, caches, l1d_factors, l1d_members, 'L1D')

def itlb_path(cores, caches):
    itlb_factors = (
        { 'set_factor': 16, 'queue_factor': 16, 'mshr_factor': 8, 'bandwidth_factor': 1 },
        { 'set_factor': 64, 'mshr_factor': 8, 'bandwidth_factor': 0.5 }
    )
    itlb_members = (
        { '_defaults': 'champsim::defaults::default_itlb' },
        { '_defaults': 'champsim::defaults::default_stlb' }
    )
    yield from default_path(cores, caches, itlb_factors, itlb_members, 'ITLB')

def dtlb_path(cores, caches):
    dtlb_factors = (
        { 'set_factor': 16, 'queue_factor': 16, 'mshr_factor': 8, 'bandwidth_factor': 1 },
        { 'set_factor': 64, 'mshr_factor': 8, 'bandwidth_factor': 0.5 }
    )
    dtlb_members = (
        { '_defaults': 'champsim::defaults::default_dtlb' },
        { '_defaults': 'champsim::defaults::default_stlb' }
    )
    yield from default_path(cores, caches, dtlb_factors, dtlb_members, 'DTLB')

def list_defaults(cores, caches):
    l1i = list(l1i_path(cores, caches))
    #print(l1i)
    yield from l1i
    yield from l1d_path(cores, caches)
    yield from itlb_path(cores, caches)
    yield from dtlb_path(cores, caches)

