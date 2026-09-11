"""Wrap only the selected solver with a pinned read-only GDB-host snapshot."""
import os
from pathlib import Path


def install_observer(protocol, campaign, task, binary):
    task = int(task)
    if task < 0:
        raise ValueError('negative task')
    campaign = Path(campaign).resolve()
    binary = str(Path(binary).resolve())
    output = campaign / 'logs' / f'{task:03d}.root-gdb.jsonl'
    original = protocol.measured

    def observe(command, *args):
        words = list(map(str, command))
        matches = [i for i, word in enumerate(words) if word == binary]
        if not matches:
            return original(command, *args)
        if len(matches) != 1:
            raise ValueError('ambiguous solver command')
        if output.exists():
            raise FileExistsError(output)
        debugger = [str(campaign/'pins/gdb'), '-q', '-nx', '-batch', '--return-child-result']
        for setting in ('pagination off', 'confirm off', 'auto-load off',
                        'debuginfod enabled off', 'disable-randomization off',
                        'startup-with-shell off'):
            debugger += ['-ex', 'set '+setting]
        debugger += ['-ex', 'python exec(compile(open('+repr(str(campaign/'pins/root_gdb.py'))+
                     ').read(), "root_gdb.py", "exec"))', '--args']
        at = matches[0]
        settings = dict(TACTIX_GDB_CONFIG=str(campaign/'pins/root-gdb-config.json'),
                        TACTIX_GDB_OUTPUT=str(output))
        prior = {key:os.environ.get(key) for key in settings}
        os.environ.update(settings)
        try:
            result = original(words[:at]+debugger+words[at:], *args)
        finally:
            for key, value in prior.items():
                if value is None:
                    os.environ.pop(key, None)
                else:
                    os.environ[key] = value
        result['root_gdb'] = (dict(path=str(output.relative_to(campaign)),
            bytes=output.stat().st_size, sha256=protocol.sha256(output))
            if output.is_file() else dict(missing=True))
        return result
    protocol.measured = observe
