#!/usr/bin/env python3
"""Seal passed adapters, accounting for configure's exact generated path rewrite."""
import difflib
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
c = root / 'benchmarks/runs/20260909-6206-schedule-continue-build'
assert not (c / 'summary.json').exists()
parents = json.loads((c / 'parents.json').read_text())
sources = list(parents['parents'])
fixture = root / 'benchmarks/probes/scratch/test-native-schedule-continue-6214.c'
new = root / sources[1]; old = root / parents['parents'][sources[1]]['source']
assert 'PASS: native stopped/continued empty snapshots' in (c / 'logs/native-corrected.log').read_text()
assert 'WEXITSTATUS(status)' in (c / 'logs/native-old-expected-failure.log').read_text()
assert (new / 'src/makefile').read_text() == (old / 'src/makefile').read_text().replace(str(old), str(new))
removed = dict(description='6215 removed its exact PID-named old-control TSV after validating header and before its later generated-makefile audit assertion. No campaign data was removed; no retained per-file cleanup metadata survived that pre-seal failure.')
source = Path(__file__).with_name('finish-schedule-continue-build-6215.py')
code = source.read_text()[source.read_text().index('inputs = {}; changes = {}; binaries = {}'):]
code = code.replace('assert not target.exists(); target.write_text(patch)', 'assert target.read_text() == patch')
code = code.replace("assert changes[sources[1]] == ['src/eventtrace.c']",
                    "assert set(changes[sources[1]]) == {'src/eventtrace.c', 'src/makefile'}")
code = code.replace("limitation='Diagnostic-only continuation.",
    "generated_makefile='Exact configure-only old/new source path substitution verified;6215 initially mistook this generated artifact for an unexpected source edit.', limitation='Diagnostic-only continuation.")
exec(compile(code, str(source), 'exec'), globals())
