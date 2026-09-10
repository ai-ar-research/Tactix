"""Apply declared solver-only diagnostics without leaking them to checkers."""
import os
import re


def with_solver_environment(measured, binary, variables):
    """Wrap a measured invocation, restoring even pre-existing values on failure.

    The owning immutable manifest supplies the exact binary and environment.
    This does not invoke a shell, change arguments or configure non-solver calls.
    """
    binary = str(binary)
    variables = dict(variables)
    for key, value in variables.items():
        selected_bve = key == 'SAT_BVE_CANDIDATE_CONFLICT'
        if (not isinstance(key, str) or
                not (selected_bve or re.fullmatch(r'(TACTIX|KISSAT)_[A-Z0-9_]+', key)) or
                not isinstance(value, str) or '\0' in value):
            raise ValueError('invalid declared solver environment')
        # This read-only observer selector is deliberately not permission for
        # SAT_* stop controls or arbitrary process environment variables.
        if selected_bve and (not re.fullmatch(r'0|[1-9][0-9]*', value) or
                             int(value) > 2**64 - 1):
            raise ValueError('invalid selected BVE conflict')

    def invoke(command, *args, **kwargs):
        if binary not in map(str, command):
            return measured(command, *args, **kwargs)
        previous = {key: os.environ.get(key) for key in variables}
        try:
            os.environ.update(variables)
            result = measured(command, *args, **kwargs)
        finally:
            for key, value in previous.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value
        return dict(result, solver_environment=dict(variables))

    return invoke
