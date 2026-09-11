"""Enable one isolated source census only inside its measured solver call."""
import os
from pathlib import Path


def install_observer(protocol, campaign, task, binary):
    task = int(task)
    if task < 0:
        raise ValueError('negative task')
    campaign = Path(campaign).resolve()
    binary = str(Path(binary).resolve())
    output = campaign / 'logs' / f'{task:03d}.root-census.tsv'
    original = protocol.measured

    def observe(command, *args):
        count = list(map(str, command)).count(binary)
        if not count:
            return original(command, *args)
        if count != 1:
            raise ValueError('ambiguous solver command')
        if output.exists():
            raise FileExistsError(output)
        prior = os.environ.get('TACTIX_ROOT_CENSUS')
        os.environ['TACTIX_ROOT_CENSUS'] = str(output)
        try:
            result = original(command, *args)
        finally:
            if prior is None:
                os.environ.pop('TACTIX_ROOT_CENSUS', None)
            else:
                os.environ['TACTIX_ROOT_CENSUS'] = prior
        result['root_census'] = (dict(path=str(output.relative_to(campaign)),
            bytes=output.stat().st_size, sha256=protocol.sha256(output))
            if output.is_file() else dict(missing=True))
        return result
    protocol.measured = observe
