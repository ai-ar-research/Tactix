#!/usr/bin/env python3
"""Fresh normal-campaign audit for the full-runtime/repeated-pop checkpoint."""
from pathlib import Path
source=Path(__file__).with_name('audit-bve-active-pop-5980.py')
code=source.read_text().replace('2026-09-08-bve-active-pop-validation.json','2026-09-08-bve-runtime-validation.json')
code=code.replace('2026-09-08-bve-active-pop.md','2026-09-08-bve-runtime-and-repeated-pop.md')
code=code.replace('(5958, 5969, 5972, 5978)','(5969, 5972, 5978, 5988)')
code=code.replace('1673402,1674992,1675144,1675162,1675706,1675800,1675801,1675845',
                  '1673402,1675162,1675706,1675801,1675945')
code=code.replace('1675645,1675774,1675706,1675800,1675801,1675845',
                  '1675774,1675800,1675845,1675945')
code=code.replace('20260908-5972-bve-sift-ancestry/heap-validation.json','20260908-5978-bve-earlier-heap/first-occurrence-validation.json')
code=code.replace('Isolated5962 repairs inactive BVE pop;54 checked fixed proofs andfour fulltest suitesPASS. BC66 expected/B0.743989x/C0.931615x;full5970 andprefix5969 run. Neutral5972 moves earlier heap divergence to462198;5978 captures fullstate. No promotion orruntime gain claim.',
                  '5962 full40 measures1.116453x fixedKissat;allcertificates/otherstats exactparent except intendedcounters. No runtimegain.5978 firstoccurrence62185 heapexact doesnotcover later8966-membertarget;new5985 repeatedobserver97/97/nativecontrolsPASS,5988 runs. No promotion.')
code=code.replace("['python3', 'benchmarks/probes/scratch/analyze-bve-active-pop-5977.py', '--self-test']",
                  "['python3', 'benchmarks/probes/scratch/validate-bve-repeat-heap-5987.py', '--self-test']")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
