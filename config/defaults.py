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

from . import util

def cache_core_defaults(cpu):
    ''' Generate the lower levels that a default core would expect for each of its caches '''
    yield { 'name': cpu.get('L1I'), 'lower_level': cpu.get('L2C') }
    yield { 'name': cpu.get('L1D'), 'lower_level': cpu.get('L2C') }
    yield { 'name': cpu.get('ITLB'), 'lower_level': cpu.get('STLB') }
    yield { 'name': cpu.get('DTLB'), 'lower_level': cpu.get('STLB') }
    yield { 'name': cpu.get('L2C'), 'lower_level': 'LLC' }
    yield { 'name': cpu.get('STLB'), 'lower_level': cpu.get('PTW') }

def ptw_core_defaults(cpu):
    ''' Generate the lower levels that a default core would expect for each of its PTWs '''
    yield { 'name': cpu.get('PTW'), 'lower_level': cpu.get('L1D') }

def list_defaults_for_core(cpu, caches):
    ''' Generate the down-path defaults that a default core would expect '''
    icache_path = itertools.tee(util.iter_system(caches, cpu.get('L1I')), 2)
    dcache_path = itertools.tee(util.iter_system(caches, cpu.get('L1D')), 2)
    itlb_path = itertools.tee(util.iter_system(caches, cpu.get('ITLB')), 2)
    dtlb_path = itertools.tee(util.iter_system(caches, cpu.get('DTLB')), 2)

    l1i_members = (
        { '_first_level': True, '_is_instruction_cache': True,
         '_defaults': 'champsim::defaults::default_l1i', '_queue_factor': 32 },
        { '_defaults': 'champsim::defaults::default_l2c', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_llc', '_queue_factor': 32 }
    )

    l1d_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_l1d', '_queue_factor': 32 },
        { '_defaults': 'champsim::defaults::default_l2c', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_llc', '_queue_factor': 32 }
    )

    itlb_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_itlb', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_stlb', '_queue_factor': 16 }
    )

    dtlb_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_dtlb', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_stlb', '_queue_factor': 16 }
    )

    def connect_translator(cache, tlb):
        return {'name': cache['name'], 'lower_translate': tlb['name']}

    return (
        map(util.chain, icache_path[0], l1i_members), #L1I path
        map(util.chain, dcache_path[0], l1d_members), #L1D path
        map(util.chain, itlb_path[0], itlb_members), #ITLB path
        map(util.chain, dtlb_path[0], dtlb_members), #DTLB path
        map(connect_translator, icache_path[1], itlb_path[1]), #L1I translation path
        map(connect_translator, dcache_path[1], dtlb_path[1]) #L1D translation path
    )

# Round-Robin recipe from itertools
def roundrobin(*paths):
    ''' Yield from each of the iterables in turn '''
    num_active = len(paths)
    nexts = itertools.cycle(iter(it).__next__ for it in paths)
    while num_active:
        try:
            for next_func in nexts:
                yield next_func()
        except StopIteration:
            # Remove the iterator we just exhausted from the cycle.
            num_active -= 1
            nexts = itertools.cycle(itertools.islice(nexts, num_active))

def list_defaults(cores, caches):
    ''' Generate the down-path defaults for all cores, merging with priority towards lower levels '''
    paths = itertools.chain(*(list_defaults_for_core(cpu, caches) for cpu in cores))
    yield from util.combine_named(reversed(list(roundrobin(*paths)))).values()
