"""Prepare one preregistered WV100 recovery and one reverse matched pair."""
import csv
import datetime
import hashlib
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[3]
PREP = Path(__file__).resolve().parent


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def write(p, d):
    p.write_text(json.dumps(d, indent=2)+'\n')


def copy_pins(source, target, manifest):
    for relative, expected in manifest['pin_sha256'].items():
        if not relative.startswith('pins/'):
            continue
        original = source / relative
        assert original.is_file() and digest(original) == expected, relative
        shutil.copy2(original, target / relative)
        assert digest(target / relative) == expected
        (target / relative).chmod(0o555 if original.stat().st_mode & 0o111 else 0o444)


def main():
    origin = ROOT/'benchmarks/runs/20261002-shared-feedback-375b621b-wv100'
    pair_origin = ROOT/'benchmarks/runs/20261003-snw16-retention-matched-diagnosis'
    recovery = ROOT/'benchmarks/runs/20261003-shared-feedback-375b621b-wv100-recovery'
    reverse = ROOT/'benchmarks/runs/20261003-snw16-retention-reverse-diagnosis'
    assert not recovery.exists() and not reverse.exists()
    first = json.loads((pair_origin/'analysis.json').read_text())
    assert first['completed_models_independently_verified']==2
    assert first['normalized_work_statistics_equal'] and first['certificate_hash_equal']
    assert (pair_origin/'exit-codes.txt').read_text().strip()=='0 0'
    protocol = dict(schema='tactix-retention-bounded-followup-preregistration-v1',
        created_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_commit='375b621ba6677600f3e77bf8383af25d256d7b67',
        first_pair_analysis=dict(path=str((pair_origin/'analysis.json').relative_to(ROOT)),
                                 sha256=digest(pair_origin/'analysis.json')),
        original_failed_campaign=str(origin.relative_to(ROOT)),
        original_failed_score_sha256=digest(origin/'score.json'),
        original_gate_status='NOT ACCEPTED; never rewritten',
        hypothesis='The original lost solve may reflect cross-window unit-cost variation; first ordered pair does not show a candidate slowdown. Causation remains unproved.',
        recovery=dict(path=str(recovery.relative_to(ROOT)), maximum_new_campaigns=1,
            criterion='Complete original100-row WV100 set, unchanged baseline/input/binary/checker/resource/cap/retention/censoring contracts; maintained scorer must accept, no missing rows or rejection. Failed original remains evidence.'),
        confirmation=dict(path=str(reverse.relative_to(ROOT)), maximum_new_pairs=1,
            order=['candidate','predecessor'],
            criterion='Both legs under original1200s caps, independent completed models/proofs, full work/certificate comparison and measured repeat spread. If timeout/counter divergence/material candidate slowdown occurs, investigate rather than repeat until passing.'),
        limits=['No automatic retry loop','No source or default tuning','No node constraint or array throttle',
                'No solver/checker allowance increase','No single-row retry substituted for the full gate',
                'No broad causal claim from one pair','No private fuzz inspection'])
    write(PREP/'retention-followup-preregistration.json', protocol)
    for p in [recovery, reverse]:
        p.mkdir()
        for n in ['pins','logs','results','provisional','claims']: (p/n).mkdir()

    m=json.loads((origin/'manifest.json').read_text())
    copy_pins(origin,recovery,m)
    shutil.copy2(origin/'matrix.tsv', recovery/'matrix.tsv')
    old_batch=(origin/'array.sbatch').read_text()
    assert str(origin) in old_batch
    (recovery/'array.sbatch').write_text(old_batch.replace(str(origin),str(recovery)))
    m['created_at']=protocol['created_at_utc']
    m['recovery_protocol']=protocol
    m['pin_sha256']={str(f.relative_to(recovery)):digest(f)
                     for f in sorted((recovery/'pins').iterdir()) if f.is_file()}
    m['pin_sha256'].update({'matrix.tsv':digest(recovery/'matrix.tsv'),
                           'array.sbatch':digest(recovery/'array.sbatch')})
    old=json.loads((origin/'manifest.json').read_text())
    assert all(m[k]==old[k] for k in ['arms','baseline','baseline_manifest','resource_contract',
                                     'input_sha256','acceptance_runtime_seconds'])
    write(recovery/'manifest.json',m)

    m=json.loads((pair_origin/'manifest.json').read_text())
    copy_pins(pair_origin,reverse,m)
    with (pair_origin/'matrix.tsv').open() as f:
        reader=csv.DictReader(f,delimiter='\t'); fields=reader.fieldnames; rows=list(reader)
    with (reverse/'matrix.tsv').open('w') as f:
        writer=csv.DictWriter(f,fieldnames=fields,delimiter='\t',lineterminator='\n')
        writer.writeheader()
        for i,row in enumerate(reversed(rows)): row['task']=str(i); writer.writerow(row)
    for name in ['job.sh','verify.py']:
        (reverse/name).write_text((pair_origin/name).read_text().replace(str(pair_origin),str(reverse)))
    m['created_at']=protocol['created_at_utc'];m['followup_protocol']=protocol
    m['hypothesis']=protocol['hypothesis'];m['scope']='One reverse candidate/predecessor pair; original failed WV100 gate retained. Order/occupancy variability remains; not a replacement full gate.'
    m['pin_sha256']={str(f.relative_to(reverse)):digest(f)
                    for f in sorted((reverse/'pins').iterdir()) if f.is_file()}
    for n in ['matrix.tsv','job.sh','verify.py']:m['pin_sha256'][n]=digest(reverse/n)
    write(reverse/'manifest.json',m)

    for p in [recovery,reverse]:
        m=json.loads((p/'manifest.json').read_text())
        for n,w in m['pin_sha256'].items(): assert digest(p/n)==w,n
    write(PREP/'retention-followup-preflight.json', dict(
        preregistration_sha256=digest(PREP/'retention-followup-preregistration.json'),
        recovery=str(recovery.relative_to(ROOT)), reverse=str(reverse.relative_to(ROOT)),
        manifests={str((p/'manifest.json').relative_to(ROOT)):digest(p/'manifest.json') for p in [recovery,reverse]},
        resources_caps_inputs_and_binaries_unchanged=True,
        pin_bindings_verified={str(p.relative_to(ROOT)):len(json.loads((p/'manifest.json').read_text())['pin_sha256']) for p in [recovery,reverse]},
        solver_runs_during_preparation=0))
    print(json.dumps(dict(recovery=str(recovery),reverse=str(reverse),preregistered=True),indent=2))


if __name__=='__main__':main()
