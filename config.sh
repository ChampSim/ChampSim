#!/usr/bin/env python3
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
    parser = argparse.ArgumentParser(description='Configure ChampSim')

    parser.add_argument('--prefix', default='.',
            help='The prefix for the ChampSim sources')
    parser.add_argument('--bindir',
            help='The directory to store the resulting executables')
    parser.add_argument('--module-dir', action='append', default=[],
            help='A directory to search for all modules. The structure is assumed to follow the same as the ChampSim repository: branch direction predictors are under `branch/`, replacement policies under `replacement/`, etc.')
    parser.add_argument('--branch-dir', action='append', default=[],
            help='A directory to search for branch direction predictors')
    parser.add_argument('--btb-dir', action='append', default=[],
            help='A directory to search for branch target predictors')
    parser.add_argument('--prefetcher-dir', action='append', default=[],
            help='A directory to search for prefetchers')
    parser.add_argument('--replacement-dir', action='append', default=[],
            help='A directory to search for replacement policies')
    parser.add_argument('files', nargs='*',
            help='A sequence of JSON files describing the configuration. The last file specified has the highest priority.')

    args = parser.parse_args()

    bindir_name = os.path.expanduser(args.bindir or os.path.join(args.prefix, 'bin'))
    objdir_name = os.path.expanduser(os.path.join(args.prefix, '.csconfig'))

    if not args.files:
        print("No configuration specified. Building default ChampSim with no prefetching.")
    config_files = itertools.product(*(util.wrap_list(parse_file(f)) for f in reversed(args.files)), ({},))

    parsed_test = parse.parse_config({'executable_name': '000-test-main'},
                branch_dir=['test/modules/branch'],
                btb_dir=['test/modules/btb'],
                pref_dir=['test/modules/prefetcher'],
                repl_dir=['test/modules/replacement']
            )
    parsed_configs = (
            parse.parse_config(*c,
                branch_dir=[*(os.path.join(d,'branch') for d in args.module_dir), *args.branch_dir],
                btb_dir=[*(os.path.join(d,'btb') for d in args.module_dir), *args.btb_dir],
                pref_dir=[*(os.path.join(d,'prefetcher') for d in args.module_dir), *args.prefetcher_dir],
                repl_dir=[*(os.path.join(d,'replacement') for d in args.module_dir), *args.replacement_dir]
            )
        for c in config_files)

    filewrite.write_files(itertools.chain(
        ((*c, bindir_name, ('src',), objdir_name) for c in parsed_configs),
        ((*parsed_test, 'test/bin', ('src','test'), '.csconfig/test'),)
    ))

# vim: set filetype=python:
