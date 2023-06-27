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

def list_defaults(cpu, caches):
    ''' Generate the down-path defaults that a default core would expect '''
    l1i_members = (
        { '_first_level': True, '_is_instruction_cache': True,
         '_defaults': 'champsim::defaults::default_l1i', '_queue_factor': 32 },
        { '_defaults': 'champsim::defaults::default_l2c', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_llc', '_queue_factor': 32 }
    )
    yield from map(util.chain, util.iter_system(caches, cpu['L1I']), l1i_members)

    l1d_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_l1d', '_queue_factor': 32 },
        { '_defaults': 'champsim::defaults::default_l2c', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_llc', '_queue_factor': 32 }
    )
    yield from map(util.chain, util.iter_system(caches, cpu['L1D']), l1d_members)

    itlb_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_itlb', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_stlb', '_queue_factor': 16 }
    )
    yield from map(util.chain, util.iter_system(caches, cpu['ITLB']), itlb_members)

    dtlb_members = (
        { '_first_level': True, '_defaults': 'champsim::defaults::default_dtlb', '_queue_factor': 16 },
        { '_defaults': 'champsim::defaults::default_stlb', '_queue_factor': 16 }
    )
    yield from map(util.chain, util.iter_system(caches, cpu['DTLB']), dtlb_members)

    icache_path = util.iter_system(caches, cpu['L1I'])
    dcache_path = util.iter_system(caches, cpu['L1D'])
    itransl_path = util.iter_system(caches, cpu['ITLB'])
    dtransl_path = util.iter_system(caches, cpu['DTLB'])

    yield from ({'name': c['name'], 'lower_translate': tlb['name']} for c,tlb in zip(icache_path, itransl_path))
    yield from ({'name': c['name'], 'lower_translate': tlb['name']} for c,tlb in zip(dcache_path, dtransl_path))
