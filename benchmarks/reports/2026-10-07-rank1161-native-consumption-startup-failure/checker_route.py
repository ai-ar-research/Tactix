#!/usr/bin/env python3
"""SLURM-only producer boundary diagnostic; never runs Walk or a fallback."""
import hashlib
import gzip
import json
import os
from pathlib import Path
import subprocess
import sys
import time
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
sys.path.insert(0,str(ROOT/'tools/bench'))
from anonymous_scratch import AnonymousScratch
sys.path.insert(0,str(ROOT/'tools/fuzz'))
from modelcheck import check_files

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def invoke(command,timeout,stdout=None):
    try:
        r=subprocess.run([str(x) for x in command],stdout=stdout or subprocess.PIPE,
                         stderr=subprocess.PIPE,timeout=timeout)
        return {'command':[str(x) for x in command],'exit':r.returncode,
                'stdout':'' if stdout else r.stdout.decode(errors='replace'),
                'stderr':r.stderr.decode(errors='replace')}
    except subprocess.TimeoutExpired:
        return {'command':[str(x) for x in command],'censored':True}

def prefix(cnf,proof,scratch,pins,expected):
    binary=scratch/'forward.binary.lsr'; decoded=scratch/'forward.text.lsr'
    started=time.monotonic(); stages=[]
    for command in ([pins['dsr-trim']['path'],'-f','-c',cnf,proof,binary],
                    [pins['dsr-compress']['path'],'-d',binary,decoded],
                    [pins['srcheck']['path'],cnf,binary,'c']):
        remaining=120-(time.monotonic()-started)
        if remaining<=0: return False,stages+[{'censored':True}]
        # Empty binary LSR is a legitimate no-line prefix. The pinned codec
        # rejects an empty input; independently check those exact zero bytes
        # with Trestle rather than asking the codec or trusting elaboration.
        if command[0]==pins['dsr-compress']['path'] and binary.stat().st_size==0:
            decoded.write_bytes(b'')
            stages.append({'operation':'empty-binary-LSR identity conversion','input_bytes':0,
                           'output_bytes':0,'empty_sha256':sha(binary),'source_cnf_sha256':sha(cnf),
                           'native_dsr_sha256':sha(proof),'requires_independent_binary_Trestle':True,'exit':0})
            continue
        stage=invoke(command,remaining); stages.append(stage)
        if stage.get('exit')!=0: return False,stages
    token='s VERIFIED UNSAT' if expected=='UNSAT' else 's VALID'
    lines={line.strip() for line in (stages[-1]['stdout']+'\n'+stages[-1]['stderr']).splitlines()}
    return token in lines and 's INVALID' not in lines,stages

def independent_lsr_control(label,text,cnf,scratch,pins):
    original=scratch/f'{label}.lsr'; binary=scratch/f'{label}.binary.lsr'
    decoded=scratch/f'{label}.decoded.lsr'
    original.write_text(text)
    started=time.monotonic(); stages=[]
    for command in ([pins['dsr-compress']['path'],'-c',original,binary],
                    [pins['dsr-compress']['path'],'-d',binary,decoded],
                    [pins['srcheck']['path'],cnf,binary,'c']):
        remaining=120-(time.monotonic()-started)
        stage=invoke(command,max(0.001,remaining)) if remaining>0 else {'censored':True}
        stages.append(stage)
        if stage.get('exit')!=0: break
    completed=len(stages)==3 and all(s.get('exit')==0 and not s.get('censored') for s in stages)
    lines={line.strip() for line in (stages[-1].get('stdout','')+'\n'+stages[-1].get('stderr','')).splitlines()}
    lossless=bool(completed and original.read_text().split()==decoded.read_text().split())
    return {'label':label,'original_lsr_text':text,'stages':stages,'completed':completed,'lossless_roundtrip':lossless,
            'status':'s VALID' if 's VALID' in lines else 's INVALID' if 's INVALID' in lines else 'unexpected',
            'binary_sha256':sha(binary) if binary.exists() else None},binary

def run_controls(scratch,pins):
    # Exact binary known-invalid empty addition and truncated record; independent SAT premise.
    # Pinned dsr-trim initializes its hash table with formula_size/2;
    # one clause is an invalid checker context, not a rejected proof.
    # Two distinct non-unit clauses are the smallest admitted SAT context.
    control=scratch/'control-dsr.cnf'; control.write_text('p cnf 2 2\n1 2 0\n-1 2 0\n')
    control_model=scratch/'control.model'; control_model.write_text('s SATISFIABLE\nv 1 2 0\n')
    assert check_files(control,control_model)[0], 'independent SAT premise control failed'
    control_model.write_text('s SATISFIABLE\nv 1 -2 0\n')
    assert not check_files(control,control_model)[0], 'corrupted model accepted'
    negative=scratch/'negative.dsr'
    control_records=[]
    for label,data in [('valid-rup-unit',b'\x01\x04\x00'),('empty-positive',b''),
                       ('invalid-empty',b'\x01\x00'),('truncated',b'\x01')]:
        negative.write_bytes(data)
        valid,stages=prefix(control,negative,scratch,pins,'UNKNOWN')
        assert all(not s.get('censored') and s.get('exit',-1)>=0 for s in stages), 'negative control censored/crashed'
        assert valid==(label in ('valid-rup-unit','empty-positive')),f'checker control failed: {label}'
        control_records.append({'label':label,'valid':valid,'stages':stages})
    # Independent Trestle controls bypass dsr-trim completely. Rejecting an
    # invalid DSR at elaboration is insufficient evidence for this checker.
    lsr_control=scratch/'control-lsr.cnf'; lsr_control.write_text('p cnf 1 1\n1 0\n')
    for label,text,expected in [('lsr-invalid-rup','2 0 0\n','s INVALID'),
                                ('lsr-valid-unit-repeat','2 1 0 1 0\n','s VALID')]:
        record,binary=independent_lsr_control(label,text,lsr_control,scratch,pins)
        record['expected_status']=expected
        control_records.append(record)
        assert record['completed'] and record['lossless_roundtrip'] and record['status']==expected, f'independent Trestle control failed: {label}'
    valid_binary=binary.read_bytes()
    assert valid_binary, 'valid compressed LSR unexpectedly empty'
    truncated=scratch/'lsr-truncated.binary.lsr'; truncated.write_bytes(valid_binary[:-1])
    stage=invoke([pins['srcheck']['path'],lsr_control,truncated,'c'],120)
    lines={line.strip() for line in (stage.get('stdout','')+'\n'+stage.get('stderr','')).splitlines()}
    record={'label':'lsr-truncated','operation':'remove final byte from authenticated valid-unit-repeat binary LSR',
            'original_sha256':hashlib.sha256(valid_binary).hexdigest(),'truncated_sha256':sha(truncated),
            'original_binary_hex':valid_binary.hex(),'truncated_binary_hex':valid_binary[:-1].hex(),
            'expected_status':'s INVALID','stages':[stage]}
    control_records.append(record)
    assert stage.get('exit')==0 and not stage.get('censored') and 's INVALID' in lines and 's VALID' not in lines, 'truncated binary LSR not independently rejected'
    return control_records
