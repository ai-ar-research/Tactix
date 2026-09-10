#!/usr/bin/env python3
"""Fresh seal after the6959 premature64/66 request was correctly refused."""
import json
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[3]
c = root/'benchmarks/runs/20260910-6957-xor-presence-BC-gate'
assert sys.argv[1:] == ['--seal']
assert not (c/'summary-6959.json').exists(), 'premature gate must remain unwritten'
tasks = {p.stem for folder in ('provisional','results') for p in (c/folder).glob('*.json')}
assert tasks == {f'{i:03d}' for i in range(66)}, 'wait for exact solver cardinality'
source = Path(__file__).with_name('validate-xor-presence-BC-6959.py')
code = source.read_text(); assert 'summary-6959.json' in code
code = code.replace('summary-6959.json','summary-6968.json')
exec(compile(code,str(source),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
with (c/'preflight-classification-6968.json').open('x') as out:
    json.dump(dict(failed_attempt='validate-xor-presence-BC-6959.py --seal',published=64,required=66,
        classification='Premature seal request hit unchanged cardinality assertion before opening summary-6959.json. No gate weakened, no solver failure or retry, no accepted artifact changed. Lasttwo tasks actually started08:21:44UTC, later than array submission08:19:22UTC; claim timestamps explain pending state. Fresh6968 seals only after all66 exact expected outcomes.'),out,indent=2);out.write('\n')
