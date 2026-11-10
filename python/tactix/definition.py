"""Read native DefaultPolicy settings without copying its defaults into Python."""
from collections.abc import Mapping
import re
from types import MappingProxyType

_KEY = re.compile(r' ([a-z_]\w*\.[\w.]+)=')
_NUMBER = re.compile(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?\Z')

class StrategyDefinition(Mapping):
    """Resolved strategy-v1 fields; authored programs remain lossless text.

    Scalar values retain integer precision or round-trip native doubles. Braced
    stage bodies and unbraced Lucky program text are kept as single values.
    This describes a policy; it neither chooses nor executes solver actions.
    """
    __slots__ = ('_text', '_values')

    def __init__(self, text):
        if not text.startswith('strategy-v1 '):
            raise ValueError('unsupported native strategy definition')
        fields, depth, at = [], 0, 0
        for match in _KEY.finditer(text):
            for char in text[at:match.start()]:
                depth += (char == '{') - (char == '}')
                if depth < 0: raise ValueError('unbalanced strategy definition')
            at = match.start()
            if not depth: fields.append(match)
        for char in text[at:]:
            depth += (char == '{') - (char == '}')
            if depth < 0: raise ValueError('unbalanced strategy definition')
        if depth or not fields: raise ValueError('incomplete strategy definition')
        values = {}
        for index, match in enumerate(fields):
            key = match[1]
            if key in values: raise ValueError(f'duplicate strategy field {key}')
            value = text[match.end():fields[index+1].start() if index+1<len(fields) else len(text)]
            if _NUMBER.fullmatch(value):
                value = float(value) if any(c in value for c in '.eE') else int(value)
            values[key] = value
        self._text, self._values = text, MappingProxyType(values)

    @property
    def text(self): return self._text
    def __getitem__(self, key): return self._values[key]
    def __iter__(self): return iter(self._values)
    def __len__(self): return len(self._values)

    def section(self, name):
        """Return that group's fields with the common prefix removed."""
        prefix = name + '.'
        return {key[len(prefix):]: value for key, value in self.items() if key.startswith(prefix)}
