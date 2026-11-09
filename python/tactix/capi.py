"""Complete typed C ABI. Functions retain the C return and ownership contracts.

Declarations are generated from the installed headers; no compiler or parser is
needed at runtime. Keep callback objects and borrowed buffers alive as required
by those headers. Session/Execution provide the managed incremental interface.
"""
import ctypes as C
import os
from . import _capi_schema as schema

_PRIMITIVES = {
    'void': None, 'char': C.c_char, 'signed char': C.c_byte, 'unsigned char': C.c_ubyte,
    'short': C.c_short, 'unsigned short': C.c_ushort, 'int': C.c_int,
    'unsigned': C.c_uint, 'unsigned int': C.c_uint, 'long': C.c_long,
    'unsigned long': C.c_ulong, 'long long': C.c_longlong,
    'unsigned long long': C.c_ulonglong, 'float': C.c_float, 'double': C.c_double,
    'size_t': C.c_size_t,
    **{prefix+str(bits)+'_t': getattr(C, 'c_'+prefix+str(bits))
       for prefix in ('int', 'uint') for bits in (8,16,32,64)},
}
_TYPES = dict(_PRIMITIVES)
for _name in schema.OPAQUE:
    _TYPES[_name] = type(_name, (C.Structure,), {})
for _name, (_kind, _, _) in schema.RECORDS.items():
    _TYPES[_name] = type(_name, (C.Union if _kind=='union' else C.Structure,), {})
_complete = set()

def _canonical(code):
    while isinstance(code, str) and code in schema.ALIASES:
        code = schema.ALIASES[code]
    return code

def _ctype(code):
    code = _canonical(code)
    if isinstance(code, str):
        if code in schema.RECORDS and code not in _complete:
            fields = schema.RECORDS[code][2]
            _TYPES[code]._fields_ = [(name, _ctype(value)) for name,value in fields]
            _complete.add(code)
        return _TYPES[code]
    kind, value, *extra = code
    if kind == '*':
        value = _canonical(value)
        if value == 'void' or isinstance(value,str) and value in schema.OPAQUE:
            return C.c_void_p
        if value == 'char': return C.c_char_p
        return C.POINTER(_TYPES[value] if isinstance(value,str) and value in schema.RECORDS else _ctype(value))
    if kind == 'array': return _ctype(value) * extra[0]
    if kind == 'function': return C.CFUNCTYPE(_ctype(value), *map(_ctype,extra[0]))
    raise ValueError('unknown ABI type: '+repr(code))

for _name in (*schema.RECORDS, *schema.ALIASES):
    _TYPES[_name] = _ctype(_name)
globals().update({name:_TYPES[name] for name in schema.PUBLIC_TYPES})
globals().update(schema.CONSTANTS)
SIGNATURES = {name:(_ctype(result), tuple(map(_ctype,args)))
              for name,(result,args) in schema.FUNCTIONS.items()}

def TACTIX_LIT_INSTANCE(literal):
    """The public macro's signed-32-bit pattern, zero-extended to uint64."""
    return C.c_uint32(literal).value

def load_library(path=None):
    """Load and type every function from tactix.h, ipasir.h and tactix_ipasir.h."""
    library = C.CDLL(str(path or os.environ.get('TACTIX_LIBRARY','libtactix_ipasir.so')))
    for name, (result,args) in SIGNATURES.items():
        function = getattr(library,name)
        function.restype, function.argtypes = result,args
    return library

class SolverView:
    """Guarded C functions bound to an Execution's borrowed solver handle."""
    def __init__(self, library, handle, check):
        self._library, self._handle, self._check = library, handle, check

    def __getattr__(self, name):
        declaration = schema.FUNCTIONS.get(name)
        global_query = name in ('tactix_rule_count','tactix_rule_name','tactix_action_kind')
        if not declaration or not global_query and (not declaration[1] or
                declaration[1][0] != ('*','tactix') or name in ('tactix_release','tactix_add')):
            raise AttributeError(name)
        function = getattr(self._library,name)
        def call(*args):
            self._check()
            return function(*args) if global_query else function(self._handle,*args)
        return call
