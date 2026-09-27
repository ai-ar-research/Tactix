#!/usr/bin/env python3
"""Prepare candidate-only automatic-proof measurements or score a frozen reference.

This tool never reruns a reference solver. Candidate gates use plan §5's
automatic PBP/DSR route through anonymous scratch. An explicit-DRAT control is
available only when a proof-backend comparison specifically requires it.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json'
MODEL_CHECKER_PINS = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/pins'
CANONICAL_CHECKERS = {
    'dsr-trim': '3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee',
    'dsr-compress': 'a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55',
    'srcheck': '19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945',
    'drat-trim': 'f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a',
    'cake_lpr': 'ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57',
    'veripb': '2e5fa5ef07c655640e6fff0d3f41561eab1502ec2e48766cca60287625935674',
    'cake-pb-cnf': 'c859a9c926ca8c63dac3317930ea8dbe3732a6fc24faa6e28ed740fadd67dc47',
}


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(8 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def reference(path=REFERENCE):
    ref = json.loads(path.read_text())
    sizes = {'tactix-fixed-rank1-40-kissat-reference-v1': 40,
             'tactix-fixed-rank1-40-reference-v1': 40,
             'tactix-fixed-rank1-50-kissat-reference-v1': 50,
             'tactix-fixed-rank1-50-reference-v1': 50,
             'tactix-fixed-b27c5-reference-v1': 32,
             'tactix-fixed-b28c5-reference-v1': 33}
    require(ref['schema'] in sizes, 'reference schema')
    count = sizes[ref['schema']]
    rows = ref['records']
    require(len(rows) == count and {r['rank'] for r in rows} == set(range(1,count+1)), 'reference rank coverage')
    require(len({r['instance'] for r in rows}) == count, 'duplicate reference input')
    if 'original_reference' in ref:
        binding = ref['original_reference']
        original_path = ROOT / binding['path']
        require(sha(original_path) == binding['sha256'], 'original reference changed')
        original = reference(original_path)
        require(rows[:40] == original['records'], 'original forty measurements changed')
        for key in ('partition','cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
            require(ref['resource_contract'][key] == original['resource_contract'][key], 'extended resource mismatch: ' + key)
    total = math.fsum(r['measurement']['solve']['wall_seconds'] for r in rows)
    require(math.isclose(total, ref['solver_seconds'], rel_tol=0, abs_tol=1e-8), 'reference total')
    return ref


def candidate_rows(ref, arms=('tactix-auto', 'tactix-drat')):
    rows = []
    for r in sorted(ref['records'], key=lambda r:r['rank']):
        for arm in arms:
            rows.append(dict(task=len(rows), rank=r['rank'], arm=arm,
                             tier=r.get('tier', 'D'),
                             instance=r['instance'], expected=r['expected']))
    return rows


def reference_identity(ref):
    return [(row['rank'], row['instance'], row['input_sha256'], row['expected'])
            for row in ref['records']]


def prepare(campaign, authority, release, bc, reference_path=REFERENCE,
            include_drat=False, runtime_target_seconds=None,
            checker_pins=None):
    campaign = campaign.resolve()
    require(campaign.parent == ROOT / 'benchmarks/runs', 'campaign must be directly under benchmarks/runs')
    require(not campaign.exists(), 'campaign already exists')
    ref = reference(reference_path)
    a = json.loads((authority / 'manifest.json').read_text())
    for rel,digest in a['files'].items():
        require(sha(ROOT / a['source'] / rel) == digest, 'candidate source changed: ' + rel)
    for binary in (release, bc):
        require(sha(binary) == a['binaries'][str(binary.resolve().relative_to(ROOT))], 'candidate pin changed')
    require(sha(release) == sha(bc), 'independent Release binaries differ')
    require(checker_pins is not None, 'explicit canonical --checker-pins required')
    checker_pins = checker_pins.resolve()
    require(checker_pins.is_relative_to(ROOT), 'checker pins must be in repository')
    for name, digest in CANONICAL_CHECKERS.items():
        path = checker_pins / name
        require(path.is_file() and sha(path) == digest,
                'canonical checker missing or changed: ' + name)
    for r in ref['records']:
        require(sha(ROOT / 'benchmarks/cache' / r['instance']) == r['input_sha256'], 'input changed')
    campaign.mkdir()
    for name in ('pins','logs','results','provisional','claims'):
        (campaign / name).mkdir()
    for name in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
        shutil.copy2(ROOT / 'tools/bench' / name, campaign / 'pins' / name)
    for name in ('checkmodel', 'modelcheck.py'):
        shutil.copy2(MODEL_CHECKER_PINS / name, campaign / 'pins' / name)
    for name in CANONICAL_CHECKERS:
        shutil.copy2(checker_pins / name, campaign / 'pins' / name)
        require(sha(campaign / 'pins' / name) == CANONICAL_CHECKERS[name],
                'copied canonical checker changed: ' + name)
    shutil.copy2(release, campaign / 'pins/tactix')
    shutil.copy2(authority / 'cumulative.patch', campaign / 'pins/candidate-source.patch')
    selected_arms = ('tactix-auto', 'tactix-drat') if include_drat else ('tactix-auto',)
    rows = candidate_rows(ref, selected_arms)
    with (campaign / 'matrix.tsv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    array = f'0-{len(rows)-1}'
    batch = f'''#!/usr/bin/env bash
#SBATCH --job-name=tactix-auto-gate
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=12
#SBATCH --mem=32768M
#SBATCH --time=05:00:00
#SBATCH --array={array}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
exec python3 {shlex.quote(str(campaign / 'pins/runtime_task.py'))} {shlex.quote(str(campaign))} "$SLURM_ARRAY_TASK_ID"
'''
    (campaign / 'array.sbatch').write_text(batch)
    arm = dict(path='pins/tactix', sha256=sha(release), source_commit=a['production'],
        source_build=str(release.parent.relative_to(ROOT)), arguments=[],
        source_patch='pins/candidate-source.patch', source_patch_sha256=sha(authority / 'cumulative.patch'))
    arms = {'tactix-auto': dict(arm, arguments=['--auto-proof'], certificate='auto')}
    if include_drat:
        arms['tactix-drat'] = dict(arm, arguments=['--drat'], certificate='drat')
    proof_mode = ('automatic PBP/DSR plus same-source DRAT' if include_drat
                  else 'automatic PBP/DSR')
    contract = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                    memory_mib=32768, array=array, throttle=None,
                    node_constraint=None, proof_mode=proof_mode,
                    one_solver_per_task=True, solve_timeout=1200,
                    checker_timeout=16000)
    if runtime_target_seconds is not None:
        require(math.isfinite(runtime_target_seconds) and
                runtime_target_seconds > 0, 'runtime target')
    manifest = dict(schema='tactix-fixed-reference-runtime-v6', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=arms, input_sha256={r['instance']:r['input_sha256'] for r in ref['records']},
        resource_contract=contract, collect_runtime_context=True,
        scratch_policy='anonymous-local-files-v1',
        scratch_contract='Linux O_TMPFILE under /tmp for CNF, proof, model and checker intermediates; kernel cleanup on exit, timeout, crash, OOM and cancellation.',
        candidate_authority=dict(path=str((authority / 'manifest.json').relative_to(ROOT)),sha256=sha(authority / 'manifest.json')),
        checker_authority=dict(path=str(checker_pins.relative_to(ROOT)),
                               sha256=CANONICAL_CHECKERS),
        fixed_reference=dict(path=str(reference_path.relative_to(ROOT)),sha256=sha(reference_path)),
        purpose=(f'{len(ref["records"])} automatic-proof solves against the retained reference vector; '
                 + ('same-source DRAT controls included; ' if include_drat else '')
                 + 'no reference solver rerun.'),
        test_consideration=(a['test_consideration'] +
            ' Current runtime protocol checks routed PBP/DSR certificates, task identity and anonymous cleanup.'),
        validation=f'Plan §5 routes startup PBP to VeriPB/CakePB and every native DSR through no-option dsr-trim/LSR, then CakeLPR for witness-free output or Trestle for witnessed output; every stage must verify. Expected solves allow provisional development; all {len(rows)} candidate results, retained reference certificates, the companion B28+C5 gate, memory and Claude fuzz are required for acceptance.',
        limitation='Cross-window fixed reference; not isolated unit-cost attribution or promotion.')
    if runtime_target_seconds is not None:
        manifest['acceptance_runtime_seconds'] = runtime_target_seconds
    manifest['pin_sha256'] = {str(p.relative_to(campaign)):sha(p)
        for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv', campaign / 'array.sbatch']}
    (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv',
              campaign / 'array.sbatch', campaign / 'manifest.json']:
        p.chmod(p.stat().st_mode & ~0o222)
    return manifest


def verify_campaign(campaign):
    manifest_path = campaign / 'manifest.json'
    require(manifest_path.is_file(), 'prepare campaign first')
    manifest = json.loads(manifest_path.read_text())
    require(manifest.get('schema') in ('tactix-fixed-reference-runtime-v2',
                                       'tactix-fixed-reference-runtime-v3',
                                       'tactix-fixed-reference-runtime-v4',
                                       'tactix-fixed-reference-runtime-v5',
                                       'tactix-fixed-reference-runtime-v6'),
            'campaign schema')
    binding = manifest['fixed_reference']
    require(sha(ROOT / binding['path']) == binding['sha256'],
            'fixed reference changed')
    for rel, digest in manifest['pin_sha256'].items():
        require(sha(campaign / rel) == digest, 'pin changed: ' + rel)
    reference(ROOT / binding['path'])
    return manifest


def submit(campaign):
    campaign = campaign.resolve()
    manifest = verify_campaign(campaign)
    launch = campaign / 'launch.json'
    require(not launch.exists(), 'campaign already submitted')
    output = subprocess.run(
        ['sbatch', '--parsable', str(campaign / 'array.sbatch')],
        check=True, capture_output=True, text=True).stdout.strip()
    job_id = output.split(';', 1)[0]
    with (campaign / 'matrix.tsv').open(newline='') as source:
        tasks = sum(1 for _ in csv.DictReader(source, delimiter='\t'))
    record = dict(job_id=job_id,
                  tasks=tasks,
                  manifest_sha256=sha(campaign / 'manifest.json'),
                  submitted_at=dt.datetime.now(dt.timezone.utc).isoformat())
    with launch.open('x') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(job_id)
    return record


def score_records(ref, records, arm='tactix', runtime_target_seconds=None,
                  require_canonical_dsr=False, require_automatic_routes=False):
    expected = {r['rank']:r for r in ref['records']}
    seen = set()
    total = 0.0
    solved = checked = 0
    per_rank = []
    for r in records:
        rank = int(r['rank'])
        require(rank in expected and rank not in seen, 'unknown/duplicate candidate rank')
        seen.add(rank)
        base = expected[rank]
        require(r['arm'] == arm and r['instance'] == base['instance'] and r['expected'] == base['expected'], 'candidate identity mismatch')
        s = r['solve']
        require(math.isfinite(s['wall_seconds']) and s['wall_seconds'] >= 0, 'invalid candidate time')
        # A clean timeout with no verdict has no certificate to reject. Keep
        # it in PAR-2 after final cleanup, just as in the provisional view.
        clean_timeout = (s['timed_out'] and r.get('verdict') == 'UNKNOWN' and
                         r.get('proof_check') is None and r.get('model_check') is None)
        require('valid' not in r or r['valid'] or clean_timeout, 'rejected candidate certificate')
        if require_canonical_dsr and arm == 'tactix-dsr' and base['expected'] == 'UNSAT' and r.get('valid'):
            check = r.get('proof_check') or {}
            names = [stage.get('name') for stage in check.get('stages', [])]
            final = {'dsr-cake_lpr': 'cake_lpr', 'dsr-trestle': 'srcheck'}.get(
                check.get('solver_reported_route'))
            require(check.get('result') == 'verified' and
                    final is not None and names[:1] == ['dsr-trim'] and
                    names[-1:] == [final] and
                    names in (['dsr-trim', final],
                              ['dsr-trim', 'dsr-compress', final]) and
                    all(stage.get('result') == 'verified' for stage in check['stages']),
                    'explicit DSR gate requires canonical checker pipeline')
        if require_automatic_routes and arm == 'tactix-auto' and base['expected'] == 'UNSAT' and r.get('valid'):
            check = r.get('proof_check') or {}
            routes = {
                'veripb-cake-pb': ['veripb', 'cake-pb-cnf'],
                'dsr-trim-dsr-compress-trestle': ['dsr-trim', 'dsr-compress', 'srcheck'],
                'dsr-trim-dsr-compress-cake_lpr': ['dsr-trim', 'dsr-compress', 'cake_lpr'],
            }
            names = [stage.get('name') for stage in check.get('stages', [])]
            expected_route = {'veripb-cake-pb': 'pbp',
                              'dsr-trim-dsr-compress-trestle': 'dsr-trestle',
                              'dsr-trim-dsr-compress-cake_lpr': 'dsr-cake_lpr'}.get(
                                  check.get('pipeline'))
            require(check.get('result') == 'verified' and
                    check.get('solver_reported_route') == expected_route and
                    names == routes.get(check.get('pipeline')) and
                    all(stage.get('result') == 'verified' for stage in check['stages']),
                    'automatic proof route differs from plan section 5')
        ok = r['expected_match'] and not s['timed_out'] and s['exit_code'] == {'SAT':10,'UNSAT':20}[base['expected']]
        solved += ok
        checked += bool(r.get('valid') and r.get('artifact_deleted') and r.get('derived_artifacts_deleted') and not r.get('cleanup_pending'))
        seconds = s['wall_seconds'] if ok else 2 * ref['resource_contract']['solve_timeout']
        total += seconds
        per_rank.append(dict(rank=rank, solved=ok, candidate_par2_seconds=seconds,
            reference_seconds=base['measurement']['solve']['wall_seconds'],
            candidate_peak_rss_kb=s.get('peak_rss_kb'),
            reference_peak_rss_kb=base['measurement']['solve'].get('peak_rss_kb')))
    complete = seen == set(expected)
    target = (1.01 * ref['solver_seconds'] if runtime_target_seconds is None
              else runtime_target_seconds)
    require(math.isfinite(target) and target > 0, 'runtime target')
    return dict(records=len(records), expected_solves=solved, checked_clean=checked,
        complete=complete, candidate_par2=total if complete else None,
        fixed_reference_seconds=ref['solver_seconds'],
        runtime_target_seconds=target,
        ratio=total / ref['solver_seconds'] if complete else None,
        runtime_target_met=bool(complete and solved == len(expected) and
                                total <= target),
        per_rank=sorted(per_rank,key=lambda r:r['rank']))


def score(campaign, comparison_reference=None, runtime_target_seconds=None):
    m = json.loads((campaign / 'manifest.json').read_text())
    binding = m['fixed_reference']
    require(sha(ROOT / binding['path']) == binding['sha256'], 'fixed reference changed')
    bound_ref = reference(ROOT / binding['path'])
    ref = bound_ref
    comparison_binding = None
    if comparison_reference is not None:
        comparison_reference = comparison_reference.resolve()
        ref = reference(comparison_reference)
        require(reference_identity(ref) == reference_identity(bound_ref),
                'comparison reference identity mismatch')
        comparison_binding = dict(
            path=str(comparison_reference.relative_to(ROOT)),
            sha256=sha(comparison_reference))
    schema = m.get('schema')
    if schema in ('tactix-fixed-reference-runtime-v2',
                  'tactix-fixed-reference-runtime-v3',
                  'tactix-fixed-reference-runtime-v4',
                  'tactix-fixed-reference-runtime-v5',
                  'tactix-fixed-reference-runtime-v6'):
        expected_arms = tuple(m['arms'])
        automatic = schema == 'tactix-fixed-reference-runtime-v6'
        main_arm = 'tactix-auto' if automatic else 'tactix-dsr'
        require(expected_arms in ((main_arm,), (main_arm, 'tactix-drat')),
                'arm mismatch')
        proof_mode = (('automatic PBP/DSR' if automatic else 'native DSR') +
                      (' plus same-source DRAT' if len(expected_arms) == 2 else ''))
        required = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                        memory_mib=32768, solve_timeout=1200,
                        checker_timeout=16000, one_solver_per_task=True,
                        proof_mode=proof_mode)
        for key, value in required.items():
            require(m['resource_contract'].get(key) == value,
                    'resource mismatch: ' + key)
        if schema == 'tactix-fixed-reference-runtime-v5':
            require(m.get('require_canonical_dsr') is True,
                    'canonical DSR gate requirement missing')
        require(m.get('scratch_policy') == 'anonymous-local-files-v1',
                'scratch policy mismatch')
        require(m['arms'][main_arm].get('certificate') == ('auto' if automatic else 'dsr') and
                m['arms'][main_arm].get('arguments') ==
                (['--auto-proof'] if automatic else []), 'main proof arm mismatch')
        if len(expected_arms) == 2:
            require(m['arms']['tactix-drat'].get('certificate') == 'drat' and
                    m['arms']['tactix-drat'].get('arguments') == ['--drat'],
                    'DRAT arm mismatch')
            require(m['arms'][main_arm]['sha256'] ==
                    m['arms']['tactix-drat']['sha256'], 'control binary mismatch')
    else:
        for key in ('cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
            require(m['resource_contract'][key] == ref['resource_contract'][key], 'resource mismatch: ' + key)
        require(m['resource_contract']['proof_mode'] == 'DRAT', 'proof mode mismatch')
        expected_arms = ('tactix',)
    require(m['input_sha256'] == {r['instance']:r['input_sha256'] for r in ref['records']}, 'input hashes mismatch')
    for rel,digest in m['pin_sha256'].items():
        require(sha(campaign / rel) == digest, 'pin changed: ' + rel)
    with (campaign / 'matrix.tsv').open(newline='') as source:
        matrix = {int(row['task']): row
                  for row in csv.DictReader(source, delimiter='\t')}
    require(len(matrix) == len(ref['records']) * len(expected_arms),
            'matrix coverage mismatch')
    records = {}
    for folder in ('provisional','results'):
        for p in (campaign / folder).glob('*.json'):
            r = json.loads(p.read_text())
            key = (r['arm'], int(r['rank']))
            task = int(r['task'])
            require(int(p.stem) == task and task in matrix,
                    'task identity mismatch')
            row = matrix[task]
            require(all(str(r[field]) == row[field]
                        for field in ('rank', 'arm', 'instance', 'expected')),
                    'matrix identity mismatch')
            if folder == 'results':
                require(r['binary_sha256'] == m['arms'][r['arm']]['sha256'], 'binary identity mismatch')
                if key in records:
                    for key in ('solve','certificate_sha256','certificate_bytes'):
                        require(records[(r['arm'], int(r['rank']))][key] == r[key], 'provisional/final mismatch')
            records[(r['arm'], int(r['rank']))] = r
    target = (m.get('acceptance_runtime_seconds')
              if runtime_target_seconds is None else runtime_target_seconds)
    arm_scores = {arm: score_records(
        ref, [record for (record_arm, _), record in records.items()
              if record_arm == arm], arm, target,
        require_canonical_dsr=(schema == 'tactix-fixed-reference-runtime-v5'),
        require_automatic_routes=(schema == 'tactix-fixed-reference-runtime-v6'))
        for arm in expected_arms}
    output = arm_scores[expected_arms[0]]
    if len(expected_arms) == 2:
        main = arm_scores[expected_arms[0]]
        drat = arm_scores['tactix-drat']
        ratio_name = ('auto_to_drat_ratio' if schema == 'tactix-fixed-reference-runtime-v6'
                      else 'dsr_to_drat_ratio')
        output = dict(main, arms=arm_scores,
                      **{ratio_name: (main['candidate_par2'] /
                                         drat['candidate_par2']
                                         if main['complete'] and drat['complete']
                                         and drat['candidate_par2'] else None)})
    reference_checked = int(ref.get('checked_at_freeze', 0))
    if ref.get('schema') == 'tactix-fixed-rank1-50-kissat-reference-v1':
        original = reference(ROOT / ref['original_reference']['path'])
        reference_checked = (int(original.get('checked_at_freeze', 0)) +
                             int(ref.get('extension_checked_at_freeze', 0)))
    for row in ref['records']:
        r = row['measurement']
        if 'source_result' in row:
            source = row['source_result']
            p = ROOT / source['path']
            if not p.exists():
                continue
            require(sha(p) == source['sha256'], 'reference result changed')
        else:
            p = ROOT / row.get('source_campaign', ref['source_campaign']) / 'results' / f'{int(r["task"]):03}.json'
        if not p.exists(): continue
        final = json.loads(p.read_text())
        require(final['valid'] and final['expected_match'], 'rejected reference certificate')
        for key in ('solve','certificate_sha256','certificate_bytes','instance','rank','arm'):
            require(final[key] == r[key], 'reference measurement changed')
        reference_checked += bool(final.get('artifact_deleted') and final.get('derived_artifacts_deleted') and not final.get('cleanup_pending'))
    output.update(reference_checked=reference_checked,
        all_certificates_checked=(reference_checked == len(ref['records']) and
            all(score['checked_clean'] == len(ref['records'])
                for score in arm_scores.values())),
        limitation='Fixed-reference cross-window score; runtime_target_met alone is not acceptance. B28+C5,current-source fuzz and memory gates remain separate.')
    output['comparison_reference'] = comparison_binding or binding
    return output


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='command', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('campaign', type=Path)
    for arg in ('authority','release','bc'):
        p.add_argument('--' + arg, type=Path, required=True)
    p.add_argument('--reference', type=Path, default=REFERENCE)
    p.add_argument('--same-source-drat', action='store_true')
    p.add_argument('--runtime-target-seconds', type=float)
    p.add_argument('--checker-pins', type=Path, required=True,
                   help='frozen automatic PBP/DSR/DRAT checker bundle')
    p = sub.add_parser('score')
    p.add_argument('campaign', type=Path)
    p.add_argument('--reference', type=Path)
    p.add_argument('--runtime-target-seconds', type=float)
    p = sub.add_parser('submit')
    p.add_argument('campaign', type=Path)
    args = ap.parse_args()
    if args.command == 'prepare':
        prepare(args.campaign.resolve(), args.authority.resolve(),
                args.release.resolve(), args.bc.resolve(),
                args.reference.resolve(), args.same_source_drat,
                args.runtime_target_seconds, args.checker_pins.resolve())
        print(args.campaign)
    elif args.command == 'score':
        print(json.dumps(score(
            args.campaign.resolve(),
            args.reference.resolve() if args.reference else None,
            args.runtime_target_seconds), indent=2))
    else:
        submit(args.campaign.resolve())


if __name__ == '__main__':
    main()
