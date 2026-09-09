#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6262-removal-observer-build'
p = json.loads((c / 'parents.json').read_text()); source = root / p['source']; parent = root / p['parent']
assert not (c / 'manifest.json').exists()
commands = [('configure', ['./configure']), ('build', ['make', '-j8'])]
for name, command in commands:
    with (c / 'logs' / (name + '.log')).open('w') as out:
        subprocess.run(command, cwd=source, stdout=out, stderr=subprocess.STDOUT, check=True)
fixture = root / 'benchmarks/probes/scratch/test-removal-observer-6263.c'
command = ['gcc', '-O2', '-I' + str(source / 'src'), '-I' + str(source / 'build'), str(fixture),
           str(source / 'build/libkissat.a'), '-lm', '-o', str(c / 'test-observer')]
with (c / 'logs/fixture.log').open('w') as out:
    subprocess.run(command, stdout=out, stderr=subprocess.STDOUT, check=True)
    subprocess.run([str(c / 'test-observer')], stdout=out, stderr=subprocess.STDOUT, check=True)
changed = [rel for rel, digest in p['parent_files'].items() if sha(source / rel) != digest]
assert set(changed) == {'src/eventtrace.c', 'src/makefile'}, changed
assert (source / 'src/makefile').read_text() == (parent / 'src/makefile').read_text().replace(str(parent), str(source))
diff = subprocess.run(['diff', '-U0', str(parent / 'src/eventtrace.c'), str(source / 'src/eventtrace.c')], capture_output=True, text=True)
assert diff.returncode == 1
(c / 'eventtrace-zero-context.patch').write_text(diff.stdout)
manifest = dict(source=p['source'], parent=p['parent'], parents_sha256=sha(c / 'parents.json'),
    files={rel: sha(source / rel) for rel in p['parent_files']}, binary_sha256=sha(source / 'build/kissat'),
    patch_sha256=sha(c / 'eventtrace-zero-context.patch'), fixture_sha256=sha(fixture),
    fixture_result=(c / 'logs/fixture.log').read_text(), commands=commands + [('fixture', command)],
    scope='Native read-only pre-garbage observer only; generated makefile path replacement verified. Full6256 replay required before interpretation; no Tactix source or accepted Kissat binary changed.')
(c / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(manifest['binary_sha256'], manifest['fixture_result'])
