import subprocess
import tempfile
import os

def check_compiles(body, *args, cxx='c++'):
    '''
    Check whether the given body compiles as a valid C++ file.
    Additional arguments to the compiler can be provided.
    '''
    with tempfile.TemporaryDirectory() as dtemp:
        fname = os.path.join(dtemp, 'temp.cc')
        with open(fname, 'wt') as wfp:
            for line in body:
                print(line, file=wfp)
        process_args = (cxx, '--std=c++17', '-c', '-o', os.devnull, *args, '-x', 'c++', fname)
        result = subprocess.run(process_args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return (result.returncode == 0)

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
    yield f'auto {name}({arg_string}){rtype_string}{" ".join(qualifiers)}'
    yield '{'
    yield from ('  '+l for l in body)
    yield '}'

def struct(name, body, superclass=None):
    '''
    Yields a C++ struct with the given name and body.

    :param name: The function name
    :param body: An iterable of function body lines
    :param superclass: The class's superclass
    '''
    superclass_string = f' : public {superclass}' if superclass is not None else ''
    yield f'struct {name}{superclass_string}'
    yield '{'
    yield from ('  '+l for l in body)
    yield '};'

