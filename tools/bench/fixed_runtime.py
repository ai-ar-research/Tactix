#!/usr/bin/env python3
"""Prepare candidate-only full40 measurements or score against a frozen reference.

This tool never submits jobs or reruns Kissat. Solver/checker execution remains
owned by the reference campaign's immutable runtime_task.py.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'benchmarks/baselines/rank1-40-kissat-12cpu-v1/manifest.json'


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
    require(ref['schema'] == 'tactix-fixed-rank1-40-kissat-reference-v1', 'reference schema')
    rows = ref['records']
    require(len(rows) == 40 and {r['rank'] for r in rows} == set(range(1,41)), 'reference rank coverage')
    require(len({r['instance'] for r in rows}) == 40, 'duplicate reference input')
    total = math.fsum(r['measurement']['solve']['wall_seconds'] for r in rows)
    require(math.isclose(total, ref['solver_seconds'], rel_tol=0, abs_tol=1e-8), 'reference total')
    return ref


def candidate_rows(ref):
    return [dict(task=i, rank=r['rank'], arm='tactix', tier='D',
                 instance=r['instance'], expected=r['expected'])
            for i,r in enumerate(sorted(ref['records'], key=lambda r:r['rank']))]


def prepare(campaign, authority, release, bc):
    campaign = campaign.resolve()
    require(campaign.parent == ROOT / 'benchmarks/runs', 'campaign must be directly under benchmarks/runs')
    require(not campaign.exists(), 'campaign already exists')
    ref = reference()
    origin = ROOT / ref['source_campaign']
    require(sha(ROOT / ref['source_manifest']['path']) == ref['source_manifest']['sha256'], 'source manifest changed')
    m = json.loads((origin / 'manifest.json').read_text())
    a = json.loads((authority / 'manifest.json').read_text())
    for rel,digest in a['files'].items():
        require(sha(ROOT / a['source'] / rel) == digest, 'candidate source changed: ' + rel)
    for binary in (release, bc):
        require(sha(binary) == a['binaries'][str(binary.resolve().relative_to(ROOT))], 'candidate pin changed')
    require(sha(release) == sha(bc), 'independent Release binaries differ')
    for rel,digest in m['pin_sha256'].items():
        require(sha(origin / rel) == digest, 'source runner/pin changed: ' + rel)
    for r in ref['records']:
        require(sha(ROOT / 'benchmarks/cache' / r['instance']) == r['input_sha256'], 'input changed')
    campaign.mkdir()
    for name in ('pins','logs','results','provisional','claims'):
        (campaign / name).mkdir()
    for name in ('runtime_task.py','drat-trim','checkmodel','modelcheck.py'):
        shutil.copy2(origin / 'pins' / name, campaign / 'pins' / name)
    shutil.copy2(release, campaign / 'pins/tactix')
    shutil.copy2(authority / 'cumulative.patch', campaign / 'pins/candidate-source.patch')
    rows = candidate_rows(ref)
    with (campaign / 'matrix.tsv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    batch = (origin / 'array.sbatch').read_text().replace(str(origin), str(campaign))
    require('#SBATCH --array=0-79' in batch, 'unexpected source array')
    batch = batch.replace('#SBATCH --array=0-79', '#SBATCH --array=0-39')
    (campaign / 'array.sbatch').write_text(batch)
    arm = dict(path='pins/tactix', sha256=sha(release), source_commit=a['production'],
        source_build=str(release.parent.relative_to(ROOT)), arguments=[],
        source_patch='pins/candidate-source.patch', source_patch_sha256=sha(authority / 'cumulative.patch'))
    contract = dict(m['resource_contract'], array='0-39', proof_mode='DRAT')
    manifest = dict(schema='tactix-fixed-reference-runtime-v1', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=dict(tactix=arm), input_sha256={r['instance']:r['input_sha256'] for r in ref['records']},
        resource_contract=contract, collect_runtime_context=True,
        candidate_authority=dict(path=str((authority / 'manifest.json').relative_to(ROOT)),sha256=sha(authority / 'manifest.json')),
        fixed_reference=dict(path=str(REFERENCE.relative_to(ROOT)),sha256=sha(REFERENCE)),
        purpose='All40 candidate-only proof-on solves against the fixed Kissat vector; no new reference arm.',
        test_consideration=a['test_consideration'] + ' Runner unchanged; fixed-runtime tests cover candidate-only matrix and score completeness/identity/check status.',
        validation='Expected solves allow provisional development; all40 candidate and reference certificates, B28+C5, memory and current-source fuzz required for acceptance.',
        limitation='Cross-window fixed reference; not isolated unit-cost attribution or promotion.')
    manifest['pin_sha256'] = {str(p.relative_to(campaign)):sha(p)
        for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv', campaign / 'array.sbatch']}
    (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for p in (campaign / 'pins').iterdir():
        p.chmod(p.stat().st_mode & ~0o222)
    return manifest


def score_records(ref, records):
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
        require(r['arm'] == 'tactix' and r['instance'] == base['instance'] and r['expected'] == base['expected'], 'candidate identity mismatch')
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
            kissat_seconds=base['measurement']['solve']['wall_seconds'],
            candidate_peak_rss_kb=s.get('peak_rss_kb'), kissat_peak_rss_kb=base['measurement']['solve'].get('peak_rss_kb')))
    complete = seen == set(range(1,41))
    return dict(records=len(records), expected_solves=solved, checked_clean=checked,
        complete=complete, candidate_par2=total if complete else None,
        fixed_kissat_seconds=ref['solver_seconds'],
        ratio=total / ref['solver_seconds'] if complete else None,
        runtime_target_met=bool(complete and solved == 40 and total <= 1.01 * ref['solver_seconds']),
        per_rank=sorted(per_rank,key=lambda r:r['rank']))


def score(campaign):
    m = json.loads((campaign / 'manifest.json').read_text())
    binding = m['fixed_reference']
    require(sha(ROOT / binding['path']) == binding['sha256'], 'fixed reference changed')
    ref = reference(ROOT / binding['path'])
    for key in ('cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
        require(m['resource_contract'][key] == ref['resource_contract'][key], 'resource mismatch: ' + key)
    require(m['resource_contract']['proof_mode'] == 'DRAT', 'proof mode mismatch')
    require(m['input_sha256'] == {r['instance']:r['input_sha256'] for r in ref['records']}, 'input hashes mismatch')
    for rel,digest in m['pin_sha256'].items():
        require(sha(campaign / rel) == digest, 'pin changed: ' + rel)
    records = {}
    for folder in ('provisional','results'):
        for p in (campaign / folder).glob('*.json'):
            r = json.loads(p.read_text())
            require(int(p.stem) == int(r['task']) == int(r['rank']) - 1, 'task identity mismatch')
            if folder == 'results':
                require(r['binary_sha256'] == m['arms']['tactix']['sha256'], 'binary identity mismatch')
                if r['rank'] in records:
                    for key in ('solve','certificate_sha256','certificate_bytes'):
                        require(records[r['rank']][key] == r[key], 'provisional/final mismatch')
            records[r['rank']] = r
    output = score_records(ref, list(records.values()))
    reference_checked = 0
    for row in ref['records']:
        r = row['measurement']
        p = ROOT / ref['source_campaign'] / 'results' / f'{int(r["task"]):03}.json'
        if not p.exists(): continue
        final = json.loads(p.read_text())
        require(final['valid'] and final['expected_match'], 'rejected reference certificate')
        for key in ('solve','certificate_sha256','certificate_bytes','instance','rank','arm'):
            require(final[key] == r[key], 'reference measurement changed')
        reference_checked += 1
    output.update(reference_checked=reference_checked,
        all_certificates_checked=reference_checked == 40 and output['checked_clean'] == 40,
        limitation='Fixed-reference cross-window score; runtime_target_met alone is not acceptance. B28+C5,current-source fuzz and memory gates remain separate.')
    return output


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='command', required=True)
    p = sub.add_parser('prepare')
    p.add_argument('campaign', type=Path)
    for arg in ('authority','release','bc'):
        p.add_argument('--' + arg, type=Path, required=True)
    p = sub.add_parser('score')
    p.add_argument('campaign', type=Path)
    args = ap.parse_args()
    if args.command == 'prepare':
        prepare(args.campaign.resolve(), args.authority.resolve(), args.release.resolve(), args.bc.resolve())
        print(args.campaign)
    else:
        print(json.dumps(score(args.campaign.resolve()), indent=2))


if __name__ == '__main__':
    main()
