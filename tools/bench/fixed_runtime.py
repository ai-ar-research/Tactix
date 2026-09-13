#!/usr/bin/env python3
"""Prepare candidate-only DSR/DRAT measurements or score a frozen reference.

This tool never submits jobs or reruns a reference solver. Each candidate gate
runs one native-DSR arm and one same-binary explicit-DRAT control through the
current anonymous-scratch runtime protocol.
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
             'tactix-fixed-b27c5-reference-v1': 32}
    require(ref['schema'] in sizes, 'reference schema')
    count = sizes[ref['schema']]
    rows = ref['records']
    require(len(rows) == count and {r['rank'] for r in rows} == set(range(1,count+1)), 'reference rank coverage')
    require(len({r['instance'] for r in rows}) == count, 'duplicate reference input')
    if count == 50:
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


def candidate_rows(ref, arms=('tactix-dsr', 'tactix-drat')):
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


def prepare(campaign, authority, release, bc, reference_path=REFERENCE):
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
    for r in ref['records']:
        require(sha(ROOT / 'benchmarks/cache' / r['instance']) == r['input_sha256'], 'input changed')
    campaign.mkdir()
    for name in ('pins','logs','results','provisional','claims'):
        (campaign / name).mkdir()
    for name in ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py'):
        shutil.copy2(ROOT / 'tools/bench' / name, campaign / 'pins' / name)
    for name in ('checkmodel', 'modelcheck.py'):
        shutil.copy2(MODEL_CHECKER_PINS / name, campaign / 'pins' / name)
    for name in ('drat-trim', 'dsr-trim', 'dsr-compress', 'srcheck'):
        source = ROOT / 'tools/checkers/bin' / name
        require(source.is_file(), 'missing checker: ' + str(source))
        shutil.copy2(source, campaign / 'pins' / name)
    shutil.copy2(release, campaign / 'pins/tactix')
    shutil.copy2(authority / 'cumulative.patch', campaign / 'pins/candidate-source.patch')
    rows = candidate_rows(ref)
    with (campaign / 'matrix.tsv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    array = f'0-{len(rows)-1}'
    batch = f'''#!/usr/bin/env bash
#SBATCH --job-name=tactix-dsr-gate
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
    arms = {
        'tactix-dsr': dict(arm, arguments=[], certificate='dsr'),
        'tactix-drat': dict(arm, arguments=['--drat'], certificate='drat'),
    }
    contract = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                    memory_mib=32768, array=array, throttle=None,
                    node_constraint=None, proof_mode='native DSR plus same-source DRAT',
                    one_solver_per_task=True, solve_timeout=1200,
                    checker_timeout=16000)
    manifest = dict(schema='tactix-fixed-reference-runtime-v2', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=arms, input_sha256={r['instance']:r['input_sha256'] for r in ref['records']},
        resource_contract=contract, collect_runtime_context=True,
        scratch_policy='anonymous-local-files-v1',
        scratch_contract='Linux O_TMPFILE under /tmp for CNF, proof, model and checker intermediates; kernel cleanup on exit, timeout, crash, OOM and cancellation.',
        candidate_authority=dict(path=str((authority / 'manifest.json').relative_to(ROOT)),sha256=sha(authority / 'manifest.json')),
        fixed_reference=dict(path=str(reference_path.relative_to(ROOT)),sha256=sha(reference_path)),
        purpose=f'{len(ref["records"])} native-DSR solves plus same-source DRAT controls against the retained reference vector; no reference solver rerun.',
        test_consideration=a['test_consideration'] + ' Current runtime protocol checks DSR elaboration/Trestle, DRAT control, task identity and anonymous cleanup.',
        validation=f'Expected solves allow provisional development; all {len(rows)} candidate results, retained reference certificates, the companion B27+C5 gate, memory and Claude fuzz are required for acceptance.',
        limitation='Cross-window fixed reference; not isolated unit-cost attribution or promotion.')
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
    require(manifest.get('schema') == 'tactix-fixed-reference-runtime-v2',
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
    record = dict(job_id=job_id,
                  tasks=len(candidate_rows(reference(
                      ROOT / manifest['fixed_reference']['path']))),
                  manifest_sha256=sha(campaign / 'manifest.json'),
                  submitted_at=dt.datetime.now(dt.timezone.utc).isoformat())
    with launch.open('x') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(job_id)
    return record


def score_records(ref, records, arm='tactix'):
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
    return dict(records=len(records), expected_solves=solved, checked_clean=checked,
        complete=complete, candidate_par2=total if complete else None,
        fixed_reference_seconds=ref['solver_seconds'],
        ratio=total / ref['solver_seconds'] if complete else None,
        runtime_target_met=bool(complete and solved == len(expected) and total <= 1.01 * ref['solver_seconds']),
        per_rank=sorted(per_rank,key=lambda r:r['rank']))


def score(campaign, comparison_reference=None):
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
    if schema == 'tactix-fixed-reference-runtime-v2':
        required = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                        memory_mib=32768, solve_timeout=1200,
                        checker_timeout=16000, one_solver_per_task=True,
                        proof_mode='native DSR plus same-source DRAT')
        for key, value in required.items():
            require(m['resource_contract'].get(key) == value,
                    'resource mismatch: ' + key)
        require(m.get('scratch_policy') == 'anonymous-local-files-v1',
                'scratch policy mismatch')
        require(set(m['arms']) == {'tactix-dsr', 'tactix-drat'}, 'arm mismatch')
        require(m['arms']['tactix-dsr'].get('certificate') == 'dsr' and
                m['arms']['tactix-dsr'].get('arguments') == [], 'DSR arm mismatch')
        require(m['arms']['tactix-drat'].get('certificate') == 'drat' and
                m['arms']['tactix-drat'].get('arguments') == ['--drat'],
                'DRAT arm mismatch')
        require(m['arms']['tactix-dsr']['sha256'] ==
                m['arms']['tactix-drat']['sha256'], 'control binary mismatch')
        expected_arms = ('tactix-dsr', 'tactix-drat')
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
    arm_scores = {arm: score_records(
        ref, [record for (record_arm, _), record in records.items()
              if record_arm == arm], arm) for arm in expected_arms}
    output = arm_scores[expected_arms[0]]
    if schema == 'tactix-fixed-reference-runtime-v2':
        dsr = arm_scores['tactix-dsr']
        drat = arm_scores['tactix-drat']
        output = dict(dsr, arms=arm_scores,
                      dsr_to_drat_ratio=(dsr['candidate_par2'] /
                                         drat['candidate_par2']
                                         if dsr['complete'] and drat['complete']
                                         and drat['candidate_par2'] else None))
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
    p = sub.add_parser('score')
    p.add_argument('campaign', type=Path)
    p.add_argument('--reference', type=Path)
    p = sub.add_parser('submit')
    p.add_argument('campaign', type=Path)
    args = ap.parse_args()
    if args.command == 'prepare':
        prepare(args.campaign.resolve(), args.authority.resolve(), args.release.resolve(), args.bc.resolve(), args.reference.resolve())
        print(args.campaign)
    elif args.command == 'score':
        print(json.dumps(score(
            args.campaign.resolve(),
            args.reference.resolve() if args.reference else None), indent=2))
    else:
        submit(args.campaign.resolve())


if __name__ == '__main__':
    main()
