#!/usr/bin/env python3
#
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

import json
import sys,os
import itertools
import argparse

import config.filewrite as filewrite
import config.parse as parse
import config.util as util

# Read the config file
def parse_file(fname):
    with open(fname) as rfp:
        return json.load(rfp)

if __name__ == '__main__':
    champsim_root = os.path.dirname(os.path.abspath(__file__))
    test_root = os.path.join(champsim_root, 'test')
    parser = argparse.ArgumentParser(description='Configure ChampSim')

    path_group = parser.add_argument_group(title='Path Configuration', description='Options that control the output locations of ChampSim configuration')

    path_group.add_argument('--prefix', default='.',
            help='The prefix for the configured outputs')
    path_group.add_argument('--bindir',
            help='The directory to store the resulting executables')

    search_group = parser.add_argument_group(title='Search Paths', description='Options that direct ChampSim to search additional paths for modules')

    search_group.add_argument('--module-dir', action='append', default=[], metavar='DIR',
            help='A directory to search for all modules. The structure is assumed to follow the same as the ChampSim repository: branch direction predictors are under `branch/`, replacement policies under `replacement/`, etc.')
    search_group.add_argument('--branch-dir', action='append', default=[], metavar='DIR',
            help='A directory to search for branch direction predictors')
    search_group.add_argument('--btb-dir', action='append', default=[], metavar='DIR',
            help='A directory to search for branch target predictors')
    search_group.add_argument('--prefetcher-dir', action='append', default=[], metavar='DIR',
            help='A directory to search for prefetchers')
    search_group.add_argument('--replacement-dir', action='append', default=[], metavar='DIR',
            help='A directory to search for replacement policies')

    parser.add_argument('--compile-all-modules', action='store_true',
            help='Compile all modules in the search path')

    parser.add_argument('files', nargs='*',
            help='A sequence of JSON files describing the configuration. The last file specified has the highest priority.')

    args = parser.parse_args()

    bindir_name = os.path.expanduser(args.bindir or os.path.join(args.prefix, 'bin'))
    objdir_name = os.path.expanduser(os.path.join(args.prefix, '.csconfig'))

    if not args.files:
        print("No configuration specified. Building default ChampSim with no prefetching.")
    config_files = itertools.product(*(util.wrap_list(parse_file(f)) for f in reversed(args.files)), ({},))

    parsed_test = parse.parse_config({'executable_name': '000-test-main'}, module_dir=[os.path.join(test_root, 'cpp', 'modules')], compile_all_modules=True)

    parsed_configs = (
            parse.parse_config(*c, module_dir=args.module_dir, branch_dir=args.branch_dir, btb_dir=args.btb_dir, pref_dir=args.prefetcher_dir, repl_dir=args.replacement_dir, compile_all_modules=args.compile_all_modules)
        for c in config_files)

    with filewrite.writer(bindir_name, objdir_name) as wr:
        for c in parsed_configs:
            wr.write_files(c)
        wr.write_files(parsed_test, bindir_name=os.path.join(test_root, 'bin'), srcdir_names=[os.path.join(test_root, 'cpp', 'src')], objdir_name=os.path.join(objdir_name, 'test'))

# vim: set filetype=python:
