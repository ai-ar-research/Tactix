#!/usr/bin/env python3
"""Build only new native observer and fixed fixtures; freeze source patch."""
import difflib
import json
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6550-native-material-observer-build'
assert not (c / 'manifest.json').exists()
p = json.loads((c / 'parents.json').read_text())
source, parent = root / p['source'], root / p['parent']
def run(name, command, cwd=None):
    with (c / 'logs' / (name + '.log')).open('x') as out:
        subprocess.run(command, cwd=cwd or root, stdout=out, stderr=subprocess.STDOUT, check=True)
    print(name, 'PASS', flush=True)
resuming_fixture = (c / 'logs/fixture-build.log').exists()
if resuming_fixture:
    assert 'solver' in (c / 'logs/fixture-build.log').read_text()
    assert 'undeclared' in (c / 'logs/fixture-build.log').read_text()
    assert {f.name for f in (c / 'logs').iterdir()} == {'configure.log', 'build.log', 'fixture-build.log'}
    assert not (c / 'fixture').exists()
    assert (source / 'build/kissat').is_file() and (source / 'build/libkissat.a').is_file()
else:
    run('configure', ['./configure'], source)
    run('build', ['make', '-j4'], source)
fixture = root / 'benchmarks/probes/scratch/test-material-observer-6552.c'
run('fixture-build-2' if resuming_fixture else 'fixture-build', ['gcc', '-O2', '-I' + str(source / 'src'), '-I' + str(source / 'build'),
    str(fixture), str(source / 'build/libkissat.a'), '-lm', '-o', str(c / 'fixture')])
run('fixture', [str(c / 'fixture'), str(c / 'fixture.tsv')])
# Re-run with the same path only to check fail-closed no-overwrite behavior;
# this deliberately expects an observer error before any result can be replaced.
saved = sha(c / 'fixture.tsv')
rejected = subprocess.run([str(c / 'fixture'), str(c / 'fixture.tsv')], cwd=root, text=True, capture_output=True)
assert rejected.returncode == 2 and sha(c / 'fixture.tsv') == saved
assert 'KISSAT_SUBSUME_MATERIAL_TRACE' in rejected.stderr
current = {str(f.relative_to(source)): sha(f) for f in (source / 'src').rglob('*') if f.is_file()}
changed = {rel for rel,digest in current.items() if p['files'].get(rel) != digest}
assert changed == {'src/eventtrace.h', 'src/eventtrace.c', 'src/inline.h', 'src/makefile'}, changed
assert (source / 'src/makefile').read_text() == (parent / 'src/makefile').read_text().replace(str(parent),str(source))
patch = ''.join(''.join(difflib.unified_diff((parent / rel).read_text().splitlines(True),
    (source / rel).read_text().splitlines(True), fromfile='a/' + rel, tofile='b/' + rel))
    for rel in sorted(changed - {'src/makefile'}))
(c / 'native.patch').write_text(patch)
with (c / 'manifest.json').open('x') as out:
    json.dump(dict(source=p['source'], parent=p['parent'], files=current,
        parents_sha256=sha(c / 'parents.json'), patch_sha256=sha(c / 'native.patch'),
        binary=dict(path=str((source / 'build/kissat').relative_to(root)), sha256=sha(source / 'build/kissat')),
        fixture=dict(source_sha256=sha(fixture), binary_sha256=sha(c / 'fixture'), output_sha256=saved,
                     stdout=(c / 'logs/fixture.log').read_text(), overwrite_rejection=dict(exit=rejected.returncode, stderr=rejected.stderr)),
        limitation='Native observer only, callback on actual false-to-true subsume marking. No Tactix source or strategy edit. Bounded ordinary-stream/proof replay required before interpreting real-instance data.'), out, indent=2)
    out.write('\n')
print(c / 'manifest.json')
