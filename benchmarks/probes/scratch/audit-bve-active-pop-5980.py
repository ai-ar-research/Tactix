#!/usr/bin/env python3
"""Fresh scoped normal-campaign checkpoint; no fuzz-owner artifact access."""
from pathlib import Path
source = Path(__file__).with_name('audit-composition-runtime-5940.py')
code = source.read_text()
changes = {
    '2026-09-08-composition-runtime-validation.json': '2026-09-08-bve-active-pop-validation.json',
    '2026-09-08-composition-runtime-and-formula.md': '2026-09-08-bve-active-pop.md',
    '(5784, 5855, 5858, 5878, 5880, 5906, 5908, 5924, 5926)': '(5784, 5855, 5858, 5878, 5880, 5906, 5908, 5924, 5926, 5968, 5970)',
    '(5906, 5908, 5924, 5926)': '(5906, 5908, 5924, 5926, 5968, 5970)',
    '(5849, 5870, 5900, 5920)': '(5900, 5920, 5962)',
    '(5917, 5925, 5931, 5932, 5937)': '(5958, 5969, 5972, 5978)',
    '1673402,1674338,1674410,1674668,1674700,1674929,1674992,1675144,1675162,1675374':
        '1673402,1674992,1675144,1675162,1675706,1675800,1675801,1675845',
    '1675145,1675329,1675330,1675374': '1675645,1675774,1675706,1675800,1675801,1675845',
    '20260908-5900-reduce-root/manifest.json': '20260908-5962-bve-active-pop/manifest.json',
    '20260908-5931-second-forward-begin-formula/summary.json': '20260908-5972-bve-sift-ancestry/heap-validation.json',
    'Reject5920 composition atfull1.176794x/C1.017340x;retain5900 at1.112038x. Neutral formula snapshots show172 incoming differences per arm before secondforward;new5937 on5900 brackets firstBVE. No implementation change orpromotion this turn.':
        'Isolated5962 repairs inactive BVE pop;54 checked fixed proofs andfour fulltest suitesPASS. BC66 expected/B0.743989x/C0.931615x;full5970 andprefix5969 run. Neutral5972 moves earlier heap divergence to462198;5978 captures fullstate. No promotion orruntime gain claim.',
    "['python3', 'benchmarks/probes/scratch/compare-first-bve-frontier-5939.py', '--self-test']":
        "['python3', 'benchmarks/probes/scratch/analyze-bve-active-pop-5977.py', '--self-test']",
}
# Simultaneous substitution avoids replacing substrings inside freshly inserted tuples.
import re
for old in changes: assert old in code, old
code = re.sub('|'.join(re.escape(k) for k in sorted(changes, key=len, reverse=True)),
              lambda m: changes[m.group()], code)
# Extend the newly generated audit, not any campaign's acceptance logic.
anchor = "exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})"
assert code.count(anchor) == 1
addition = '''
anchor_timeout = "        else:\\n            assert number == 5784"
assert code.count(anchor_timeout) == 1
code = code.replace(anchor_timeout, "        elif r.get('proof_check', {}).get('timed_out'):\\n            assert checker_timeout(r, m['resource_contract']['checker_timeout']), p\\n            counts['checker_timeout_unverified'] += 1\\n            unscored.append(dict(path=str(p.relative_to(root)), sha256=sha(p), arm=r['arm'], reason='checker_timeout'))\\n" + anchor_timeout)
prefix = "import importlib.util\\nsp = importlib.util.spec_from_file_location('checker_timeout', root / 'benchmarks/probes/scratch/classify-checker-timeout-5981.py')\\nmod = importlib.util.module_from_spec(sp); sp.loader.exec_module(mod); mod.fixtures()\\nchecker_timeout = mod.checker_timeout\\n"
code = code.replace("campaigns = []", prefix + "campaigns = []")
'''
code = code.replace(anchor, addition + '\n' + anchor)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
