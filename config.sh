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

import config.filewrite
import config.parse
import config.util

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
    path_group.add_argument('--makedir',
            help='The directory to store the resulting makefile fragment. Note that `make` must later be invoked with -I.')

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

    parser.add_argument('--no-compile-all-modules', action='store_false', dest='compile_all_modules',
            help='Do not compile all modules in the search path')
    parser.add_argument('--compile-all-modules', action='store_true', dest='compile_all_modules',
            help='Compile all modules in the search path')

    parser.add_argument('-v', action='store_true', dest='verbose')

    parser.add_argument('--join', choices=['chain','product'], default='product',
            help='The joining method when multiple files are specified. A "chain" join concatenates the files, building the union of all specifications. A "product" join merges each possible combination of the specified builds. In the case of "product", the last file specified has the highest priority.')

    parser.add_argument('files', nargs='*',
            help='A sequence of JSON files describing the configuration.')

    args = parser.parse_args()

    bindir_name = os.path.expanduser(args.bindir or os.path.join(args.prefix, 'bin'))
    objdir_name = os.path.expanduser(os.path.join(args.prefix, '.csconfig'))

    if not args.files:
        print("No configuration specified. Building default ChampSim with no prefetching.")
    files = map(config.util.wrap_list, map(parse_file, reversed(args.files)))

    if args.join == 'product':
        config_files = itertools.product(*files, ({},))
    elif args.join == 'chain':
        config_files = ((c,) for c in itertools.chain(*files))

    parsed_test = config.parse.parse_config({'executable_name': '000-test-main'}, module_dir=[os.path.join(test_root, 'cpp', 'modules')], compile_all_modules=True)

    parse_args = {
        'module_dir': args.module_dir,
        'branch_dir': args.branch_dir,
        'btb_dir': args.btb_dir,
        'pref_dir': args.prefetcher_dir,
        'repl_dir': args.replacement_dir,
        'compile_all_modules': args.compile_all_modules,
        'verbose': args.verbose
    }
    parsed_configs = (config.parse.parse_config(*c, **parse_args) for c in config_files)

    with config.filewrite.FileWriter(bindir_name=bindir_name, objdir_name=objdir_name, makedir_name=args.makedir, verbose=args.verbose) as wr:
        for c in parsed_configs:
            wr.write_files(c)

# vim: set filetype=python:
