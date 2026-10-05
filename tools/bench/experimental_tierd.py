#!/usr/bin/env python3
"""Pinned experimental Tier D with anonymous local certificates and formulas."""
import argparse
import csv
import datetime as dt
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
FROZEN_DSR=ROOT/'benchmarks/runs/20260924-am369-condition-explicit-claude-fuzz/pins'
FROZEN_DSR_HASHES={
    'dsr-trim':'3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee',
    'dsr-compress':'a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55',
    'srcheck':'19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945',
}
PROOF_ARGUMENTS={'--auto-proof','--dsr','--drat','--lrat'}
GF2_TELEMETRY_ARGUMENTS=('--feedback','--feedback-replay','--structural-provenance',
                         '--controller-gf2-affine-profile')
DEFAULT_GF2_FEEDBACK_MAX_BYTES=512 << 20
DEFAULT_GF2_FEEDBACK_DECODED_MAX_BYTES=8 << 30
DEFAULT_GF2_FEEDBACK_MAX_RECORDS=20_000_000
DEFAULT_GF2_PROVENANCE_MAX_BYTES=256 << 20
MAX_GF2_FEEDBACK_BYTES=512 << 20
MAX_GF2_FEEDBACK_DECODED_BYTES=8 << 30
MAX_GF2_FEEDBACK_RECORDS=20_000_000
MAX_GF2_PROVENANCE_BYTES=256 << 20

def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1<<20),b''):h.update(b)
    return h.hexdigest()

def save(p,data):
    with p.open('x') as f:json.dump(data,f,indent=2);f.write('\n')

def verify(c):
    m=json.loads((c/'manifest.json').read_text())
    for rel,digest in m['pin_sha256'].items():
        if sha(c/rel)!=digest:raise ValueError('pin mismatch: '+rel)
    return m

def checker_sources(reference, baseline, certificate):
    if certificate not in ('dsr','auto'):
        raise ValueError('unsupported certificate mode: '+certificate)
    names={'checkmodel':'checkmodel','modelcheck.py':'modelcheck.py',
           'drat-trim':'drat-trim','cake_lpr':'cake-lpr'}
    if certificate=='auto':
        names.update({'veripb':'veripb','cake-pb-cnf':'cake-pb-cnf'})
    sources={}
    for target,name in names.items():
        source=reference/baseline['checkers'][name]
        expected=baseline['provenance'][name]['sha256']
        if not source.is_file() or sha(source)!=expected:
            raise ValueError('frozen winner checker pin mismatch: '+name)
        sources[target]=source
    for name,expected in FROZEN_DSR_HASHES.items():
        source=FROZEN_DSR/name
        if not source.is_file() or sha(source)!=expected:
            raise ValueError('frozen AM369 DSR checker pin mismatch: '+name)
        sources[name]=source
    return sources

def scheduler_wall_seconds(solve_timeout,checker_timeout):
    # Preserve the whole solve + checker allowance and one cleanup margin.
    seconds=solve_timeout+checker_timeout+900
    return ((seconds+899)//900)*900

def slurm_time(seconds):
    return f'{seconds//3600:02d}:{(seconds//60)%60:02d}:{seconds%60:02d}'

def solver_arguments(certificate, extra, gf2_telemetry=False):
    if certificate not in ('dsr','auto'):
        raise ValueError('unsupported certificate mode: '+certificate)
    proof='--auto-proof' if certificate=='auto' else '--dsr'
    if any(not value or value in PROOF_ARGUMENTS for value in extra):
        raise ValueError('extra solver arguments must be nonempty and may not override the certificate mode')
    if gf2_telemetry and any(
            value == reserved or value.startswith(reserved+'=')
            for value in extra for reserved in GF2_TELEMETRY_ARGUMENTS):
        raise ValueError('GF2 telemetry paths are task-local and may not be overridden by solver arguments')
    return [proof,*extra]

def prepare(a):
    c=a.campaign.resolve(); ref=ROOT/'benchmarks/baselines/sc24-26-32g-5000s'
    revision=a.binary_source_commit or 'HEAD'
    commit=subprocess.check_output(['git','rev-parse',revision],cwd=ROOT,text=True).strip()
    # Every archived source/build input must be exactly the committed blob.
    source=a.source.resolve()
    listing=subprocess.check_output(['git','ls-tree','-rz',commit,'--','src','include','CMakeLists.txt'],cwd=ROOT)
    sources={}
    for entry in listing.split(b'\0'):
        if not entry:continue
        metadata,name=entry.split(b'\t',1);mode,kind,oid=metadata.split();name=name.decode()
        p=source/name
        if kind!=b'blob' or not p.is_file():continue
        actual=subprocess.check_output(['git','hash-object',str(p)],cwd=ROOT).strip()
        if actual!=oid:raise ValueError('source differs from HEAD: '+name)
        sources[name]=sha(p)
    if not all(str(p.relative_to(source)) in sources for p in (source/'src').rglob('*') if p.is_file()):
        raise ValueError('uncommitted source in build tree')
    manifest=json.loads((ref/'manifest.json').read_text())
    assert sha(ref/'manifest.json')==json.loads((ref/'launch.json').read_text())['manifest_sha256']
    checkers=checker_sources(ref,manifest,a.certificate)
    assert sha(ref/'cache.sha256')==manifest['cache_manifest_sha256']
    inputs=dict((rel,digest) for digest,rel in (line.split() for line in (ref/'cache.sha256').read_text().splitlines()))
    source_rows=[dict(rank=i+1,arm='tactix',tier='D',instance=r['rel'],expected=r['expected']) for i,r in enumerate(manifest['instances'])]
    assert len(source_rows)==1166 and len({r['instance'] for r in source_rows})==1166
    selection_authority=None
    selected_from_table={}
    if a.selection_table:
        table=a.selection_table.resolve()
        if not table.is_relative_to(ROOT):raise ValueError('selection table must be under repository root')
        with table.open(newline='') as f:
            lines=(line for line in f if line.strip() and not line.startswith('#'))
            reader=csv.DictReader(lines,delimiter='\t')
            required={'coverage_rank','original_tierd_rank','instance','verdict'}
            if not reader.fieldnames or not required.issubset(reader.fieldnames):
                raise ValueError('selection table schema mismatch')
            for row in reader:
                instance=row['instance']
                if instance in selected_from_table:raise ValueError('duplicate selection-table instance: '+instance)
                selected_from_table[instance]=row
        selection_authority=dict(path=str(table.relative_to(ROOT)),sha256=sha(table))
    if a.instance or selected_from_table:
        selected=set(a.instance) if a.instance else set(selected_from_table)
        if a.instance and len(selected)!=len(a.instance):raise ValueError('duplicate --instance')
        known={r['instance'] for r in source_rows}
        missing=selected-known
        if missing:raise ValueError('unknown Tier-D instance: '+sorted(missing)[0])
        source_rows=[r for r in source_rows if r['instance'] in selected]
    if selected_from_table:
        missing={r['instance'] for r in source_rows}-set(selected_from_table)
        if missing:raise ValueError('selected instance missing from selection table: '+sorted(missing)[0])
        for row in source_rows:
            selected=selected_from_table[row['instance']]
            if int(selected['original_tierd_rank'])!=row['rank']:
                raise ValueError('selection-table Tier-D rank mismatch: '+row['instance'])
            row['rank']=int(selected['coverage_rank'])
            row['tier']=a.selection_table.stem
            row['expected']=selected['verdict']
        source_rows.sort(key=lambda row:row['rank'])
    rows=[dict(task=i,**r) for i,r in enumerate(source_rows)]
    assert rows and len({r['instance'] for r in rows})==len(rows)
    inputs={rel:digest for rel,digest in inputs.items() if rel in {r['instance'] for r in rows}}
    assert set(inputs)=={r['instance'] for r in rows}
    assert all((ROOT/'benchmarks/cache'/rel).is_file() for rel in inputs)
    arm_arguments=solver_arguments(a.certificate,a.solver_argument,a.gf2_telemetry)
    if a.gf2_telemetry:
        profile=a.gf2_affine_profile.resolve()
        if not profile.is_file():raise ValueError('GF2 affine profile is not a file')
        arm_arguments.append(f'--controller-gf2-affine-profile={c}/pins/gf2-affine.profile')
    c.mkdir()
    directories=['pins','logs','claims','provisional','results']
    if a.retain_failed_proof:directories.append('retained-proofs')
    if a.gf2_telemetry:directories.append('telemetry')
    for name in directories:(c/name).mkdir()
    shutil.copy2(a.binary,c/'pins/tactix')
    if a.gf2_telemetry:shutil.copy2(profile,c/'pins/gf2-affine.profile')
    for name,checker_source in checkers.items():
        shutil.copy2(checker_source,c/'pins'/name)
    runner_pins=['runtime_task.py','proof_oracle.py','anonymous_scratch.py',
                 'test_anonymous_scratch.py','validate_anonymous_runtime.py']
    if a.gf2_telemetry:
        runner_pins.extend(['gf2_target_score.py','inspect_policy_feedback.py'])
    for name in runner_pins:
        shutil.copy2(ROOT/'tools/bench'/name,c/'pins'/name)
    with (c/'matrix.tsv').open('x',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t');w.writeheader();w.writerows(rows)
    common=f'''#!/usr/bin/env bash
#SBATCH --partition={a.partition}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={a.cpus_per_task}
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --output={c}/logs/%x-%A_%a.log
'''
    wall=scheduler_wall_seconds(a.solve_timeout,a.checker_timeout)
    (c/'array.sbatch').write_text(common+f'''#SBATCH --job-name=tierd-{commit[:8]}
#SBATCH --time={slurm_time(wall)}
#SBATCH --array=0-{len(rows)-1}{f'%{a.array_throttle}' if a.array_throttle else ''}
set -euo pipefail
ulimit -c 0
exec python3 {c}/pins/runtime_task.py {c} "$SLURM_ARRAY_TASK_ID"
''')
    (c/'validate.sbatch').write_text(common+f'''#SBATCH --job-name=tierd-anonymous-check
#SBATCH --time=00:05:00
set -euo pipefail
ulimit -c 0
python3 {c}/pins/test_anonymous_scratch.py
exec python3 {c}/pins/validate_anonymous_runtime.py {c}
''')
    m=dict(schema=('tactix-experimental-single-tierd-v4-gf2-telemetry'
                   if a.gf2_telemetry else 'tactix-experimental-single-tierd-v3'),
        created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        source_commit=commit,source_tree=str(source),source_files=sources,
        arms={'tactix':dict(path='pins/tactix',sha256=sha(c/'pins/tactix'),
                           arguments=arm_arguments,
                           certificate=a.certificate)},
        input_sha256=inputs,scratch_policy='anonymous-local-files-v1',
        dataset_authority=dict(path=str(ref/'manifest.json'),sha256=sha(ref/'manifest.json')),
        selection_authority=selection_authority,
        resource_contract=dict(partition=a.partition,cpus_per_task=a.cpus_per_task,memory_mib=32768,solve_timeout=a.solve_timeout,
            checker_timeout=a.checker_timeout,scheduler_wall_seconds=wall,
            array=f'0-{len(rows)-1}{f"%{a.array_throttle}" if a.array_throttle else ""}',
            throttle=a.array_throttle,node_constraint=None),
        proof_contract=('Automatic PBP / witnessed DSR / witness-free DSR routed by pinned VeriPB+CakePB, no-option dsr-trim→Trestle for witnessed SR, or validated DSR→DRAT→drat-trim→CakeLPR for witness-free DSR against the original CNF.'
                        if a.certificate=='auto' else
                        'Explicit native DSR uses pinned no-option dsr-trim→Trestle for witnessed SR, or validated DSR→DRAT→drat-trim→CakeLPR for witness-free DSR against the original CNF.'),
        cleanup_contract='O_TMPFILE on /tmp for input, DSR, LSR, model and checker scratch; no directory entries ever. Kernel releases storage at last close, including SIGKILL/OOM. Fail closed if unsupported. Only bounded JSON metadata and scheduler logs persist; core dumps disabled.',
        caveat='If the entire job dies, its result may be missing; never interpret missing as timeout or checked. Local disk may fill during a live run; no home-quota certificate storage or failure retention.',
        pin_sha256={str(p.relative_to(c)):sha(p) for p in [*(c/'pins').iterdir(),c/'matrix.tsv',c/'array.sbatch',c/'validate.sbatch']})
    if a.retain_failed_proof:
        m['failed_proof_retention']=dict(
            schema='tactix-failed-proof-retention-v1',
            directory='retained-proofs',
            maximum_bytes=a.retain_failed_proof_max_bytes,
            purpose='checker-only recovery; retain only after an unsuccessful UNSAT proof check')
        m['cleanup_contract'] += (' An unsuccessfully checked UNSAT certificate may be copied '
            'after checking into the bounded retained-proofs directory for checker-only recovery; '
            'the result records its exact SHA-256 and remains cleanup-pending.')
        m['caveat']=('Failed certificates are retained only when their size is within the declared '
            'per-task cap; solver or job death before the copy still leaves no artifact.')
    if a.gf2_telemetry:
        m['gf2_telemetry']=dict(
            schema='tactix-experimental-gf2-telemetry-v2',directory='telemetry',
            feedback_compression='gzip',feedback_wire_version=2,
            feedback_stored_max_bytes=a.gf2_feedback_max_bytes,
            feedback_decoded_max_bytes=a.gf2_feedback_decoded_max_bytes,
            feedback_max_records=a.gf2_feedback_max_records,
            structural_provenance_max_bytes=a.gf2_provenance_max_bytes)
    save(c/'manifest.json',m)
    for p in [*(c/'pins').iterdir(),c/'manifest.json',c/'matrix.tsv',c/'array.sbatch',c/'validate.sbatch']:p.chmod(p.stat().st_mode&~0o222)
    verify(c); print(c)

def submit(a):
    c=a.campaign.resolve();verify(c)
    name='validation-launch.json' if a.validate else 'launch.json'
    assert not (c/name).exists()
    if not a.validate:assert json.loads((c/'validation.json').read_text())['passed']
    script=c/('validate.sbatch' if a.validate else 'array.sbatch')
    command=['sbatch','--parsable']
    if a.afterany is not None:command.append(f'--dependency=afterany:{a.afterany}')
    job=subprocess.check_output([*command,str(script)],text=True).strip().split(';')[0]
    save(c/name,dict(job_id=job,manifest_sha256=sha(c/'manifest.json'),submitted_at=dt.datetime.now(dt.timezone.utc).isoformat(),
                     dependency_afterany=a.afterany))
    print(job)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--campaign',type=Path,required=True)
    p.add_argument('--source',type=Path)
    p.add_argument('--binary',type=Path)
    p.add_argument('--binary-source-commit')
    p.add_argument('--partition',default='cpu-q')
    p.add_argument('--cpus-per-task',type=int,default=4)
    p.add_argument('--solve-timeout',type=int,default=1200)
    p.add_argument('--checker-timeout',type=int,default=16000)
    p.add_argument('--array-throttle',type=int)
    p.add_argument('--afterany',type=int,help='submit only after this SLURM job or array terminates')
    p.add_argument('--certificate',choices=['dsr','auto'],default='dsr')
    p.add_argument('--solver-argument',action='append',default=[],
                   help='repeatable immutable solver argument for the prepared arm; use = before values beginning with --')
    p.add_argument('--gf2-telemetry',action='store_true',
                   help='retain and score bounded GF(2) feedback/provenance sidecars')
    p.add_argument('--gf2-affine-profile',type=Path,
                   help='GF(2) producer profile frozen into an opt-in campaign')
    p.add_argument('--gf2-feedback-stored-max-bytes','--gf2-feedback-max-bytes',
                   dest='gf2_feedback_max_bytes',type=int,
                   default=DEFAULT_GF2_FEEDBACK_MAX_BYTES)
    p.add_argument('--gf2-feedback-decoded-max-bytes',type=int,
                   default=DEFAULT_GF2_FEEDBACK_DECODED_MAX_BYTES)
    p.add_argument('--gf2-feedback-max-records',type=int,
                   default=DEFAULT_GF2_FEEDBACK_MAX_RECORDS)
    p.add_argument('--gf2-provenance-max-bytes',type=int,
                   default=DEFAULT_GF2_PROVENANCE_MAX_BYTES)
    p.add_argument('--retain-failed-proof',action='store_true',
                   help='retain a bounded copy of an UNSAT certificate after checker failure')
    p.add_argument('--retain-failed-proof-max-bytes',type=int,default=1 << 30,
                   help='maximum failed certificate bytes retained per task')
    p.add_argument('--instance',action='append',default=[])
    p.add_argument('--selection-table',type=Path)
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument('--prepare',action='store_true');g.add_argument('--validate',action='store_true');g.add_argument('--submit',action='store_true')
    a=p.parse_args()
    if a.cpus_per_task < 1:p.error('--cpus-per-task must be positive')
    if a.solve_timeout < 1:p.error('--solve-timeout must be positive')
    if a.checker_timeout < 1:p.error('--checker-timeout must be positive')
    if a.array_throttle is not None and a.array_throttle < 1:
        p.error('--array-throttle must be positive')
    if a.afterany is not None and (a.afterany < 1 or a.prepare):
        p.error('--afterany requires a positive job id and --submit or --validate')
    if a.solver_argument and not a.prepare:
        p.error('--solver-argument is valid only with --prepare')
    if not 1 <= a.gf2_feedback_max_bytes <= MAX_GF2_FEEDBACK_BYTES:
        p.error(f'--gf2-feedback-stored-max-bytes must be in 1..{MAX_GF2_FEEDBACK_BYTES}')
    if not 1 <= a.gf2_feedback_decoded_max_bytes <= MAX_GF2_FEEDBACK_DECODED_BYTES:
        p.error(f'--gf2-feedback-decoded-max-bytes must be in 1..{MAX_GF2_FEEDBACK_DECODED_BYTES}')
    if not 1 <= a.gf2_feedback_max_records <= MAX_GF2_FEEDBACK_RECORDS:
        p.error(f'--gf2-feedback-max-records must be in 1..{MAX_GF2_FEEDBACK_RECORDS}')
    if not 1 <= a.gf2_provenance_max_bytes <= MAX_GF2_PROVENANCE_BYTES:
        p.error(f'--gf2-provenance-max-bytes must be in 1..{MAX_GF2_PROVENANCE_BYTES}')
    if (not a.gf2_telemetry and
            (a.gf2_feedback_max_bytes != DEFAULT_GF2_FEEDBACK_MAX_BYTES or
             a.gf2_feedback_decoded_max_bytes != DEFAULT_GF2_FEEDBACK_DECODED_MAX_BYTES or
             a.gf2_feedback_max_records != DEFAULT_GF2_FEEDBACK_MAX_RECORDS or
             a.gf2_provenance_max_bytes != DEFAULT_GF2_PROVENANCE_MAX_BYTES)):
        p.error('GF2 telemetry byte caps require --gf2-telemetry')
    if a.gf2_telemetry and not a.prepare:
        p.error('--gf2-telemetry is valid only with --prepare')
    if a.retain_failed_proof and not a.prepare:
        p.error('--retain-failed-proof is valid only with --prepare')
    if a.retain_failed_proof_max_bytes < 1:
        p.error('--retain-failed-proof-max-bytes must be positive')
    if a.gf2_telemetry != bool(a.gf2_affine_profile):
        p.error('--gf2-affine-profile is required exactly when --gf2-telemetry is selected')
    prepare(a) if a.prepare else submit(a)
