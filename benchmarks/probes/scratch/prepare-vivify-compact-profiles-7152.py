#!/usr/bin/env python3
"""Predeclare three matched Rank35 complete-Vivify sampled profile pairs."""
import csv
import datetime
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools/bench'))
from fixed_runtime import sha
from checked_comparison import classify
from perf_callstack import call_sites
reader = Path(__file__).with_name('submit-vivify-compact-wave-7150.py')
spec = importlib.util.spec_from_file_location('bve_fingerprint', reader)
h = importlib.util.module_from_spec(spec); spec.loader.exec_module(h); h.fixtures()
c = root / 'benchmarks/runs/20260910-7152-vivify-compact-profiles'
assert not c.exists()
runtime = root / 'benchmarks/runs/20260910-7150-vivify-compact-runtime'
old = root / 'benchmarks/runs/20260910-7060-rank35-residual-profile'
m = json.loads((runtime / 'manifest.json').read_text())
om = json.loads((old / 'manifest.json').read_text())
deps = dict(m['dependencies'])
def bind(p, expected=None):
    value = sha(p); assert expected is None or value == expected, p
    deps[str(p.relative_to(root))] = value
for parent, manifest in ((runtime, m), (old, om)):
    bind(parent / 'manifest.json')
    for rel, digest in manifest['pin_sha256'].items(): bind(parent / rel, digest)
for rel, digest in m['protected_campaign_hashes'].items(): bind(root / rel, digest)
gate = root / 'benchmarks/runs/20260910-7149-vivify-compact-BC-gate'
for p in (gate / 'results').glob('*.json'):
    assert classify(json.loads(p.read_text()), True)['status'] in ('checked_SAT', 'checked_UNSAT'), p
oracles, seeds = {}, {}
for task, arm in ((2, 'candidate'), (3, 'previous')):
    p = runtime / 'results' / f'{task:03d}.json'; final = p.exists()
    if not final: p = runtime / 'provisional' / p.name
    if not p.exists():
        print('First unprofiled Rank35 pair pending; no output created'); sys.exit(0)
    r = json.loads(p.read_text()); bind(p)
    assert classify(r, final)['status'] in ('pending_validation', 'checked_UNSAT'), p
    assert r['rank'] == 35 and r['arm'] == arm and r['task'] == task
    frozen = m['previous_work_oracles']['35']; f = root / frozen['path']; bind(f, frozen['sha256'])
    assert h.fingerprint(r) == h.fingerprint(json.loads(f.read_text()))
    oracles['35/' + arm] = dict(path=str(p.relative_to(root)), sha256=sha(p))
    seeds[arm] = dict(rank=35, tier='D', instance=r['instance'], expected='UNSAT')
entry = 'tactix::DefaultInprocessScheduler::run_phase(tactix::Kernel&, tactix::SlotGroup, int volatile*)'
caller = 'tactix::DefaultPolicy::solve(tactix::Kernel&, int volatile*)'
expected = {
    'candidate': {0x507a93: ('initial_probe', 'xor %edx,%edx'), 0x508343: ('probe', 'xor %edx,%edx'), 0x508ee2: ('eliminate', 'mov $0x1,%edx')},
    'previous': {0x507793: ('initial_probe', 'xor %edx,%edx'), 0x508043: ('probe', 'xor %edx,%edx'), 0x508be2: ('eliminate', 'mov $0x1,%edx')},
}
assemblies, site_evidence = {}, {}
for arm in ('candidate', 'previous'):
    a = m['authorities'][arm]; p = root / a['path']; bind(p, a['sha256'])
    build = json.loads(p.read_text())
    for rel, digest in build['files'].items(): bind(root / build['source'] / rel, digest)
    binary = runtime / m['arms'][arm]['path']
    assemblies[arm] = subprocess.check_output(['objdump', '-dC', '--no-show-raw-insn',
        '--disassemble=' + caller, str(binary)], text=True)
    sites, evidence = call_sites(assemblies[arm], entry, expected[arm])
    site_evidence[arm] = dict(sites=sites, calls=evidence)
replay = root / 'benchmarks/reports/2026-09-10-bve-lifecycle-reader-replay-7131.json'
validation = json.loads(replay.read_text())
assert validation['status'] == 'PASS' and validation['exact_frozen_attribution']
for rel, digest in validation['hashes'].items(): bind(root / rel, digest)
bind(replay); bind(reader); bind(Path(__file__)); bind(root / 'tools/bench/perf_callstack.py')
for name in ('pins', 'logs', 'results', 'provisional', 'claims', 'profiles'):
    (c / name).mkdir(parents=True)
for name in ('runtime_task.py', 'drat-trim', 'checkmodel', 'modelcheck.py', 'candidate', 'previous'):
    shutil.copy2(runtime / 'pins' / name, c / 'pins' / name)
wrapper = (old / 'pins/run.py').read_text()
before = "('candidate', 'kissat')"; after = "('candidate', 'previous')"
assert wrapper.count(before) == 1
wrapper = wrapper.replace(before, after)
assert wrapper.replace(after, before) == (old / 'pins/run.py').read_text()
(c / 'pins/run.py').write_text(wrapper)
rows = []
for wave in range(3):
    for arm in (('previous', 'candidate') if wave == 1 else ('candidate', 'previous')):
        rows.append(dict(task=len(rows), arm=arm, **seeds[arm]))
with (c / 'matrix.tsv').open('x', newline='') as stream:
    writer = csv.DictWriter(stream, rows[0], delimiter='\t', lineterminator='\n')
    writer.writeheader(); writer.writerows(rows)
batch = (runtime / 'array.sbatch').read_text().replace(str(runtime), str(c))
batch = batch.replace('--array=0-17', '--array=0-5').replace('7150-vivify-compact-runtime', '7152-vivify-compact')
batch = batch.replace('/pins/runtime_task.py', '/pins/run.py')
(c / 'array.sbatch').write_text(batch)
for arm, body in assemblies.items(): (c / (arm + '-solve-disassembly.txt')).write_text(body)
out = dict(schema='tactix-vivify-whole-lifecycle-three-wave-profiles-v1',
    created_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), arms=m['arms'],
    authorities=m['authorities'], dependencies=deps, protected_campaign_hashes=m['protected_campaign_hashes'],
    input_sha256={r['instance']: m['input_sha256'][r['instance']] for r in rows},
    resource_contract=dict(m['resource_contract'], array='0-5', scheduler_wall_seconds=18000),
    collect_runtime_context=True, profile=om['profile'], unprofiled_records=oracles,
    call_sites=site_evidence, phase_entry=entry, phase_caller=caller,
    source_review='Both immutable sources have identical DefaultPolicy call sites: startup Probe, search Probe, search Eliminate. Bind all three machine calls and group-argument setup; never infer phase from sample frequency or numerical PC order.',
    purpose='Assess at least15% complete-Vivify Rank35 sampled-cycle reduction on7144 versus7030 in three independent matched pairs; include scheduling, root preparation, watch rebuild and probing descendants of vivify_apply, not only the printed timer or sort symbol; no Kissat rerun.',
    submission_contract='Three sequential full-width2-task waves0-1,2-3,4-5; candidate/control concurrent, middle wave reversed, no throttle/node constraint. Proceed after exact expected solver outcomes while proofs continue.',
    validation='All6 exact own unprofiled work/proof fingerprints and repeated own results. Hash-bound self/callpath profiles, zero lost samples, exact total-period conservation, no unresolved phase caller; independently checked proofs and clean final records. Unprofiled7150 owns total-runtime/RSS acceptance; sampling overhead is not solver memory.',
    test_consideration='Unchanged measured/proof/cleanup wrapper body except explicit candidate/previous solver aliases; use the already tested7150 runtime protocol.15 callstack tests include7 new aggregation tests; require exact replay of frozen7082 current attribution before new interpretation. No solver change or new solver unit test required.',
    limitation='Whole-function sampled cycles, not exact per-operation latency. Preserve all paired ratios and variance; no repeat-to-select. No-phase ancestry is not pure CDCL; inclusive symbols overlap. Pending proofs permit provisional continuation only. Full50/fuzz/consolidation and memory gates remain.')
out['pin_sha256'] = {str(p.relative_to(c)): sha(p) for p in [*(c / 'pins').iterdir(), c / 'matrix.tsv', c / 'array.sbatch', *(c.glob('*-solve-disassembly.txt'))]}
with (c / 'manifest.json').open('x') as stream: json.dump(out, stream, indent=2); stream.write('\n')
for p in (c / 'pins').iterdir(): p.chmod(p.stat().st_mode & ~0o222)
print(c)
