#!/usr/bin/env python3
"""Fresh normal-campaign audit; preserve all previous snapshot authorities."""
from pathlib import Path
source=Path(__file__).with_name('audit-composition-runtime-5940.py')
code=source.read_text().replace('2026-09-08-composition-runtime-validation.json','2026-09-08-first-bve-attempts-validation.json')
code=code.replace('2026-09-08-composition-runtime-and-formula.md','2026-09-08-first-bve-attempts.md')
code=code.replace('(5917, 5925, 5931, 5932, 5937)','(5937, 5945, 5949)')
code=code.replace('1675144,1675162,1675374','1675144,1675162,1675374,1675510,1675537')
code=code.replace('1675145,1675329,1675330,1675374','1675374,1675510,1675537')
code=code.replace('Reject5920 composition atfull1.176794x/C1.017340x;retain5900 at1.112038x. Neutral formula snapshots show172 incoming differences per arm before secondforward;new5937 on5900 brackets firstBVE. No implementation change orpromotion this turn.',
                  'Retain5900 at1.112038x;5937 classifies nine inactive attempts and816 incoming literal poses.5949 replayPASS localizes earlier same-pop heap position4465/4472 at122629.5945 cancelled deliberately,corrected5948 actual lifecycle controlsPASS. No solver implementation orpromotion.')
code=code.replace("['python3', 'benchmarks/probes/scratch/compare-first-bve-frontier-5939.py', '--self-test']",
                  "['python3', 'benchmarks/probes/scratch/analyze-first-bve-attempts-5943.py', '--self-test']")
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
