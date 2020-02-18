#!/usr/bin/env python

import os
import argparse

cwd = os.getcwd()
champsim_dir = os.path.dirname(os.path.realpath(__file__))

# path-based parameters
path_keys = ['l1prefetcher', 'l2prefetcher', 'llprefetcher', 'llreplacement', 'branchpredictor']

# keys that should be appended, not replaced
app_keys = ['cc', 'cxx', 'cflags', 'cxxflags', 'cppflags', 'ldflags', 'ldlibs']

parser = argparse.ArgumentParser(description='Configures ChampSim before building.')

parser.add_argument('-v', '--verbose', action='store_true', help='Configure with debugging output')

parser.add_argument('-1', '--l1prefetcher', help='Use the given L1 prefetcher')
parser.add_argument('-2', '--l2prefetcher', help='Use the given L1 prefetcher')
parser.add_argument('-3', '--llprefetcher', help='Use the given LLC prefetcher')
parser.add_argument('-r', '--llreplacement', help='Use the given LLC replacement policy')
parser.add_argument('-b', '--branchpredictor', help='Use the given branch predictor')

parser.add_argument('-n', dest='num_cores', help='Build with the given number of cores')

parser.add_argument('-o', dest='executable_name', help='The executable to be created')

parser.add_argument('--cflags', action='append', type=str, help='Add the given options to CFLAGS')
parser.add_argument('--cxxflags', action='append', type=str, help='Add the given options to CXXFLAGS')
parser.add_argument('--cppflags', action='append', type=str, help='Add the given options to CPPFLAGS')
parser.add_argument('--ldflags', action='append', type=str, help='Add the given options to LDFLAGS')
parser.add_argument('--ldlibs', action='append', type=str, help='Add the given options to LDLIBS')

parser.add_argument('config_file', type=argparse.FileType('rt'), help='The configuration file')

# Get values from arguments
args = parser.parse_args()

# Defaults
params = {}

# Check environment
for key in app_keys:
    if key.upper() in os.environ:
        params[key] = os.environ[key.upper()]

# Read config file
if 'config_file' in vars(args):
    for line in args.config_file:
        line = line.strip()
        if (len(line) == 0): continue
        if (line[0] == '#'): continue
        key, val = tuple(line.split('=',1))

        # Handle the compiler flags as a special "append case"
        if key.lower() not in app_keys:
            params[key.lower()] = val
        else:
            params[key.lower()] = params.get(key.lower(), '') + ' ' + val

# Override with values from arguments
for key,val in vars(args).items():
    if val is not None:
        # keys are lower here to match the params dict
        if key not in app_keys:
            params[key] = ' ' + str(val)
        else:
            for v in val:
                params[key] = params.get(key, '') + ' ' + v

# Adding multicore options
params['cppflags'] = params.get('cppflags', '') + ' -DNUM_CPUS={}'.format(params['num_cores'])

# Adding verbosity
if args.verbose:
    params['cflags'] = params.get('cflags', '') + ' -g'
    params['cxxflags'] = params.get('cxxflags', '') + ' -g'
    params['cppflags'] = params.get('cppflags', '') + ' -DDEBUG_PRINT -UNDEBUG'

# Fully qualify paths
for key in path_keys:
    if key in params:
        params[key] = os.path.expanduser(params[key])
        params[key] = os.path.expandvars(params[key])
        params[key] = os.path.realpath(params[key])
        params[key] = os.path.join(cwd, params[key])

with open(os.path.join(champsim_dir, 'configure.mk'), 'wt') as wfp:
    wfp.write('L1PREFETCHER=' + params.get('l1prefetcher', os.path.join(champsim_dir, 'prefetcher', 'no.l1d_pref')) + '\n')
    wfp.write('L2PREFETCHER=' + params.get('l2prefetcher', os.path.join(champsim_dir, 'prefetcher', 'no.l2c_pref')) + '\n')
    wfp.write('LLPREFETCHER=' + params.get('llprefetcher', os.path.join(champsim_dir, 'prefetcher', 'no.llc_pref')) + '\n')
    wfp.write('LLREPLACEMENT=' + params.get('llreplacement', os.path.join(champsim_dir, 'replacement', 'lru.llc_repl')) + '\n')
    wfp.write('BRANCH_PREDICTOR=' + params.get('branchpredictor', os.path.join(champsim_dir, 'branch', 'bimodal.bpred')) + '\n')

    if 'executable_name' in params:
        wfp.write('executable_name=' + params['executable_name'] + '\n')

    if 'cc' in params:
        wfp.write('CC=' + params['cc'] + '\n')

    if 'cxx' in params:
        wfp.write('CXX=' + params['cxx'] + '\n')

    if 'cflags' in params:
        wfp.write('CFLAGS=' + params['cflags'] + '\n')

    if 'cxxflags' in params:
        wfp.write('CXXFLAGS=' + params['cxxflags'] + '\n')

    if 'cppflags' in params:
        wfp.write('CPPFLAGS=' + params['cppflags'] + '\n')

    if 'ldflags' in params:
        wfp.write('LDFLAGS=' + params['ldflags'] + '\n')

    if 'ldlibs' in params:
        wfp.write('LDLIBS=' + params['ldlibs'] + '\n')

print('Configured for ' + params.get('executable_name', 'champsim'))

# vim: filetype=python:

