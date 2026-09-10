"""Build a narrow control trace and check its keys against pinned sources.

This checks literal getenv call sites, not arbitrary C/C++ semantics. Exact
solver replay remains mandatory. The finite event endpoint is intentionally
unreachable: a separate prefix stop owns the diagnostic's termination.
"""
import re

UNREACHABLE_FINITE_STOP = (1 << 64) - 2


def validate_trace_keys(env, prefix, source):
    if prefix not in ('TACTIX', 'KISSAT'):
        raise ValueError('unknown solver prefix')
    reads = set(re.findall(r'\bgetenv\s*\(\s*"([^"]+)"', source))
    selected = {key for key in env if key.startswith(prefix + '_EVENT_TRACE')}
    unknown = selected - reads
    if unknown:
        raise ValueError('trace settings absent from pinned getenv calls: ' + ', '.join(sorted(unknown)))
    return sorted(selected)


def control_window(prefix, path, window, prefix_stop, cap, source):
    if len(window) != 2 or not 0 <= window[0] <= window[1] < prefix_stop < UNREACHABLE_FINITE_STOP:
        raise ValueError('invalid window or competing prefix endpoint')
    if not path or cap <= 0:
        raise ValueError('invalid path or row cap')
    suffixes = dict(EVENT_TRACE=str(path),
        EVENT_TRACE_STOP_AFTER_CONFLICT=str(UNREACHABLE_FINITE_STOP),
        EVENT_TRACE_CONTROL_ONLY='1',
        EVENT_TRACE_CONFLICT_FROM=str(window[0]),
        EVENT_TRACE_CONFLICT_TO=str(window[1]),
        EVENT_TRACE_SKIP_WATCH='1', EVENT_TRACE_SKIP_IMPLY='1',
        EVENT_TRACE_SKIP_HEAP='1', EVENT_TRACE_SKIP_RANDEC_DRAWS='1',
        EVENT_TRACE_LIMIT=str(cap))
    env = {prefix + '_' + key: value for key, value in suffixes.items()}
    validate_trace_keys(env, prefix, source)
    return env
