#!/usr/bin/env python3
"""Record actual propagation codegen before attributing an unexpected slowdown."""
import collections
import json
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha

def instructions(raw,start,size):
    rows=[]
    for line in raw.splitlines():
        match=re.match(r'^\s*([0-9a-f]+):\s+(\S.*)$',line)
        if not match: continue
        pc=int(match[1],16); assert start<=pc<start+size
        assert not rows or pc>rows[-1]['pc']
        rows.append(dict(pc=pc,offset=pc-start,text=match[2].strip()))
    assert rows and rows[0]['pc']==start
    return rows

def fixtures():
    raw='header\n  100: mov %rdi,%rax\n  103: callq 200 <function>\n'
    assert len(instructions(raw,0x100,8))==2
    for text,start,size in ((raw,0x101,8),(raw,0x100,3),('',0,1),
        (raw+'  103: ret\n',0x100,8),(raw+'  102: ret\n',0x100,8)):
        try: instructions(text,start,size)
        except AssertionError: pass
        else: raise AssertionError('invalid instruction boundary accepted')
    print('Static instruction parser: positive and five invalid boundaries PASS')

def main():
    assert sys.argv[1:] in ([],['--self-test'])
    fixtures()
    if sys.argv[1:]: return
    c=root/'benchmarks/runs/20260911-7276-realloc-propagation-code'
    assert not c.exists(),'successful or failed evidence retained'
    c.mkdir()
    hashes={}; records={}
    def bind(p,expected=None):
        value=sha(p); assert expected is None or value==expected,p
        hashes[str(p.relative_to(root))]=value
    symbols=('_ZN6tactix16kernel_propagateERNS_6KernelEj',
             '_ZN6tactix23kernel_propagate_binaryERNS_6KernelEb')
    # Discover companion manglings from the pinned symbol table, not guesses.
    sources={}
    for arm,stem,authority in (('previous','7253-occurrence-cutover','7253-occurrence-cutover'),
                               ('candidate','7267-dense-realloc','7271-dense-realloc')):
        p=root/f'benchmarks/runs/20260911-{authority}-build/manifest.json'
        build=json.loads(p.read_text()); bind(p)
        binary=root/f'build/pin-{stem}-release/tactix'
        bind(binary,build['binaries'][str(binary.relative_to(root))])
        source=root/build['source']; sources[arm]=source
        for rel in ('src/kernel/kernel.cpp','src/kernel/watch.hpp'):
            bind(source/rel,build['files'][rel])
        nm=subprocess.check_output(['nm','-S',str(binary)],text=True)
        selected=[line.split() for line in nm.splitlines() if len(line.split())==4
                  and line.split()[2]=='T' and 'kernel_propagate' in line.split()[3]]
        assert len(selected)==4
        records[arm]={}
        for address,size,kind,symbol in selected:
            command=['objdump','-d','--no-show-raw-insn','--disassemble='+symbol,str(binary)]
            raw=subprocess.check_output(command,text=True)
            rows=instructions(raw,int(address,16),int(size,16))
            calls=[row for row in rows if re.match(r'^callq?\s',row['text'])]
            records[arm][symbol]=dict(address=address,size=int(size,16),rows=rows,
                instruction_count=len(rows),calls=calls,command=command,raw_disassembly=raw)
    assert set(records['previous'])==set(records['candidate'])
    for rel in ('src/kernel/kernel.cpp','src/kernel/watch.hpp'):
        assert sha(sources['previous']/rel)==sha(sources['candidate']/rel)
    comparison={symbol:{arm:{k:records[arm][symbol][k] for k in ('address','size','instruction_count','calls')}
                for arm in records} for symbol in records['previous']}
    bind(Path(__file__))
    with (c/'summary.json').open('x') as out:
        json.dump(dict(hashes=hashes,records=records,comparison=comparison,
            source_unchanged=['src/kernel/kernel.cpp','src/kernel/watch.hpp'],
            interpretation='Ordinary propagation machine body changes despite identical propagation/watch source: storage-template changes affect emitted code. Smaller static body is not evidence of lower dynamic cost. Current Rank38 wave0 slowdown lies outside the inprocessing slot; repetitions and matched profiles must locate any reproducible cost.',
            limitation='No alignment tuning, compiler-heuristic causality claim, whole-program equivalence or performance acceptance. Raw instructions and operands preserved; only diagnostic boundary parsing is performed.'),out,indent=2)
    print(json.dumps(comparison,indent=2))

if __name__=='__main__': main()
