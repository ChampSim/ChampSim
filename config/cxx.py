import subprocess
import tempfile
import os

from . import util

class CompileResult:
    '''
    The result from check_compiles(), which is convertible to boolean, but retains the output from the compilation check.
    '''
    def __init__(self, subprocess_result):
        self.returncode = subprocess_result.returncode
        self.args = subprocess_result.args
        self.stdout = subprocess_result.stdout
        self.stderr = subprocess_result.stderr

    def __bool__(self):
        return self.returncode == 0

def check_compiles(body, *args, cxx=None):
    '''
    Check whether the given body compiles as a valid C++ file.
    Additional arguments to the compiler can be provided.
    '''
    cxxflags = [*filter(None, os.environ.get('CXXFLAGS','').split()), '--std=c++17', '-fsyntax-only']
    cppflags = [*filter(None, os.environ.get('CPPFLAGS','').split())]
    with tempfile.TemporaryDirectory() as dtemp:
        fname = os.path.join(dtemp, 'temp.cc')
        with open(fname, 'wt') as wfp:
            for line in body:
                print(line, file=wfp)
        process_args = (cxx or os.environ.get('CXX', 'c++'), *cppflags, *cxxflags, '-o', os.devnull, *args, '-x', 'c++', fname)
        result = subprocess.run(
            process_args,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True,
            check=False
        )
        return CompileResult(result)

def brace_wrap(body):
    ''' Wrap and indent the iterable. '''
    yield '{'
    yield from ('  '+l for l in body)
    yield '}'

def function(name, body, args=None, rtype=None, qualifiers=tuple()):
    '''
    Yields a C++ function with the given name and body.

    :param name: The function name
    :param body: An iterable of function body lines
    :param args: An iterable of (type, name) pairs
    :param rtype: The return type (auto if not specified)
    :param qualifiers: An iterable of type qualifiers (e.g. const, override)
    '''
    local_args = args or tuple()
    arg_string = ', '.join((a[0]+' '+a[1]) for a in local_args)
    rtype_string = f' -> {rtype}' if rtype is not None else ''
    qualifier_string = (' '+' '.join(qualifiers)) if qualifiers else ''
    yield f'auto {name}({arg_string}){rtype_string}{qualifier_string}'
    yield from brace_wrap(body)

def struct(name, body, superclass=None):
    '''
    Yields a C++ struct with the given name and body.

    :param name: The function name
    :param body: An iterable of function body lines
    :param superclass: The class's superclass
    '''
    superclass_string = f' : public {superclass}' if superclass is not None else ''
    yield f'struct {name}{superclass_string}'
    head, tail = util.cut(brace_wrap(body), n=-1)
    yield from head
    yield from (l+';' for l in tail)
