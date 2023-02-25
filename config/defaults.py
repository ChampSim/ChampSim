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

