#!/usr/bin/env python3
"""Prepared review/build/task/grade commands. No jobs are submitted here."""
import argparse
import hashlib
import json
import lzma
import os
from pathlib import Path
import resource
import subprocess
import sys
import tarfile
import time

D = Path(__file__).resolve().parent
ROOT = D.parents[3]
M = json.loads((D/'manifest.json').read_text())
def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def verify():
    assert sha(M['input']) == M['input_compressed_sha256']
    assert sha(D/'driver.cpp') == M['driver_source_sha256']
    assert sha(D/'solver-headers-76e1f68b8.tar') == M['headers_archive_sha256']
    assert sha(D/'frozen-objects.rsp') == M['link_response_sha256']
    for artifact, digest in M['artifacts'].items():
        assert sha(D/artifact) == digest, artifact
    for path, digest in M['frozen_link_inputs'].items():
        assert sha(path) == digest, path
    for authority, digest in M['authority_files'].items():
        assert sha(ROOT/authority) == digest, authority
    for item in M['checkers'].values():
        assert sha(item['path']) == item['sha256'], item['path']
    for arm, profile in M['profiles'].items():
        assert sha(profile['path']) == profile['sha256'], arm
        assert sha(D/arm/'pin/tactix') == M['solver_sha256']

def build():
    verify()
    headers = D/'frozen-headers'
    headers.mkdir(exist_ok=True)
    with tarfile.open(D/'solver-headers-76e1f68b8.tar') as archive:
        archive.extractall(headers)
    compiler = '/cm/local/apps/gcc/13.1.0/bin/c++'
    compile_cmd = [compiler, '-std=c++20', '-O3', '-DNDEBUG', '-Wall', '-Wextra',
                   '-fno-exceptions', '-fno-rtti', '-I'+str(headers/'src'),
                   '-I'+str(headers/'include'), '-c', str(D/'driver.cpp'),
                   '-o', str(D/'driver.o')]
    # Frozen headers retain Release NDEBUG; driver locally enables fail-closed checks.
    subprocess.run(compile_cmd, check=True)
    link_cmd = [compiler, str(D/'driver.o'), '@'+str(D/'frozen-objects.rsp'),
                '-o', str(D/'driver')]
    subprocess.run(link_cmd, check=True)
    (D/'driver-build.json').write_text(json.dumps({
        'source_commit': M['source_commit'], 'compile': compile_cmd,
        'link': link_cmd, 'driver_source_sha256': sha(D/'driver.cpp'),
        'driver_object_sha256': sha(D/'driver.o'),
        'driver_sha256': sha(D/'driver'),
        'frozen_link_inputs': M['frozen_link_inputs'],
    }, indent=2)+'\n')

def run_stage(name, cmd, output, deadline):
    remaining = deadline-time.monotonic()
    assert remaining > 0, 'whole checker deadline exhausted'
    with output.open('wb') as log:
        result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT,
                                timeout=remaining)
    text = output.read_text(errors='replace')
    return {'stage': name, 'command': list(map(str,cmd)),
            'exit': result.returncode, 'sha256': sha(output),
            'unsat_token': 's VERIFIED UNSAT' in text.splitlines(),
            'valid_token': any(x in text.splitlines() for x in
                               ['s VALID', 's VERIFIED UNSAT'])}

def task(arm):
    verify()
    assert os.environ.get('SLURM_JOB_ID'), 'real solver runs require SLURM'
    build_record = json.loads((D/'driver-build.json').read_text())
    assert sha(D/'driver') == build_record['driver_sha256']
    out = D/arm/'results'
    out.mkdir(exist_ok=False)
    resource.setrlimit(resource.RLIMIT_FSIZE, (M['limits']['file_bytes'],)*2)
    cnf = out/'input.cnf'
    with lzma.open(M['input'],'rb') as source, cnf.open('wb') as target:
        for block in iter(lambda:source.read(1<<20),b''):
            target.write(block)
    sys.path.insert(0,str(ROOT/'tools/bench'))
    from proof_oracle import canonical_checker_cnf, check_dsr
    spelling = canonical_checker_cnf(cnf,out/'checker.cnf')
    proof = out/'proof.dsr'
    cmd = [str(D/'driver'),str(cnf),str(proof),str(out/'provenance.jsonl'),
           str(out/'transcript'),arm,str(D/'off.program'),'30']
    with (out/'stdout').open('wb') as stdout, (out/'stderr').open('wb') as stderr:
        process = subprocess.run(cmd,stdout=stdout,stderr=stderr,timeout=35)
    record = {'command':cmd, 'exit':process.returncode,
              'input':spelling,'driver_sha256':sha(D/'driver'),
              'proof_sha256':sha(proof), 'proof_bytes':proof.stat().st_size}
    # Audit the ENTIRE retained stream, including UNKNOWN. Both verified
    # checkers explicitly support VALID non-refuting prefixes. That token
    # receives zero SAT/UNSAT credit and is never passed to verdict grading.
    deadline = time.monotonic()+M['limits']['whole_checker_seconds']
    dsr = M['checkers']['dsr-trim']['path']
    sr = M['checkers']['srcheck']['path']
    bad_cnf=out/'known-sat.cnf'; bad_cnf.write_text('p cnf 1 1\n1 0\n')
    bad_proof=out/'known-bad.dsr'; bad_proof.write_text('0\n')
    negative=run_stage('known-bad-prefix', [dsr,'-f',str(bad_cnf),str(bad_proof),
        str(out/'known-bad.lsr')],out/'known-bad.log',deadline)
    assert negative['exit'] != 0 and not negative['valid_token'], negative
    forward=run_stage('every-lemma-full-prefix',[dsr,'-f',str(out/'checker.cnf'),
        str(proof),str(out/'forward.lsr')],out/'forward.log',deadline)
    assert forward['exit']==0 and forward['valid_token'], forward
    independent=run_stage('Trestle-full-prefix',[sr,str(out/'checker.cnf'),
        str(out/'forward.lsr')],out/'Trestle.log',deadline)
    assert independent['exit']==0 and independent['valid_token'], independent
    assert not forward['unsat_token'] and not independent['unsat_token'], 'refutation on frozen SAT input'
    record['prefix_stages']=[negative,forward,independent]
    record['prefix_valid']=True
    record['verdict_credit']=False
    if process.returncode == 20:
        # Canonical no-option DSR/Trestle route is additional to full forward
        # prefix checking and must succeed against the same original spelling.
        ok,stage,text=check_dsr(dsr,M['checkers']['dsr-compress']['path'],sr,
            cnf,proof,out/'canonical',timeout=max(0,deadline-time.monotonic()))
        (out/'canonical.log').write_text(text)
        assert ok, stage
        record['canonical_unsat']=True
        # Rank401 is frozen SAT: even a checked opposite answer fails this gate.
        raise AssertionError('wrong verdict for frozen SAT Rank401')
    if process.returncode == 10:
        model=run_stage('original-model',[str(ROOT/'tools/fuzz/checkmodel'),
            str(cnf),str(out/'stdout')],out/'model.log',deadline)
        assert model['exit']==0,model
        record['verdict_credit']=True
    assert process.returncode in (0,10), process.returncode
    (out/'result.json').write_text(json.dumps(record,indent=2)+'\n')

def grade():
    records=[]; common=[]
    for arm in ('off','on'):
        out=D/arm/'results'; record=json.loads((out/'result.json').read_text())
        lines=(out/'transcript').read_text().splitlines()
        boundary=lines.index('COMMON-END')
        # Includes every regenerated typed request/result, exact coordinate
        # definitions, explicit pending propagation and common root snapshot.
        common.append(lines[2:boundary])
        assert sum(x.startswith('owner-coordinate ') for x in common[-1])==3
        assert any(x.startswith('common-root') for x in common[-1])
        assert record['prefix_valid'] and record['proof_bytes']<=M['limits']['file_bytes']
        assert lines[-1].startswith('verdict ')
        end=next(x for x in lines if x.startswith('END '))
        installed=[x for x in lines if x.startswith('X20-installed ')]
        assert len(installed)==(3 if arm=='on' else 0)
        records.append({'arm':arm,'end':end,'new_binaries':installed,
                        'direct_bcp_events':sum(x.startswith('X20-BCP ') for x in lines),
                        'direct_conflict_events':sum(x.startswith('X20-direct-conflict ') for x in lines),
                        'verdict_credit':record['verdict_credit']})
    assert common[0]==common[1], 'common source-bound prefix diverged; unscorable'
    (D/'pair-grade.json').write_text(json.dumps({
        'common_prefix_identical':True,'records':records,
        'gate':'admission/proof + narrow direct consumption only',
        'direct_consumption_observed':any(r['direct_bcp_events'] or r['direct_conflict_events'] for r in records),
        'long_run_authorized':False,
        'limitations':['not transitive-resolution credit','not solve-speed evidence',
                       'external driver has not qualified DefaultPolicy scheduling']
    },indent=2)+'\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('command',choices=['review','build','task','grade'])
    parser.add_argument('--arm',choices=['off','on'])
    args=parser.parse_args()
    if args.command=='review': verify(); print('Frozen inputs verified; not submitted.')
    elif args.command=='build': build()
    elif args.command=='task': assert args.arm; task(args.arm)
    else: grade()
