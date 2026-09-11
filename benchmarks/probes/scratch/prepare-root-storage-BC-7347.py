#!/usr/bin/env python3
"""Use the actual structured CTest logs; no campaign was created by 7346."""
from pathlib import Path
base = Path(__file__).with_name('prepare-root-storage-BC-7346.py')
code = base.read_text().replace('7346', '7347')
for before, after in {
    "f'logs/{kind}-ctest.log'": "f'logs/{kind}-ctest.json'",
    "assert f'100% tests passed out of {count}' in path.read_text(); bind(path)":
        "log = json.loads(path.read_text()); assert log['returncode'] == 0\n    assert f'100% tests passed out of {count}' in log['stdout']; bind(path)",
    "for path in [Path(__file__),": "for path in [Path(__file__),Path(__file__).with_name('prepare-root-storage-BC-7346.py'),Path(__file__).with_name('check-submit-root-storage-BC-7346.py'),Path(__file__).with_name('occurrence-fingerprint-7256.py'),",
}.items():
    assert before in code, before
    code = code.replace(before, after)
exec(compile(code, str(base), 'exec'), {'__file__':str(Path(__file__)), '__name__':'__main__'})
