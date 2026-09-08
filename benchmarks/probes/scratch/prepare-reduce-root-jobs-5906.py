#!/usr/bin/env python3
"""Fresh independent Reduce-guidance BC and complete Rank32 replay campaigns."""
from pathlib import Path
import re
source=Path(__file__).with_name('prepare-forward-admission-jobs-5836.py')
code=source.read_text()
changes={
    '20260908-5830-forward-admission':'20260908-5900-reduce-root',
    '20260908-5806-forward-unit-BC-gate':'20260908-5855-forward-caller-BC-gate',
    '20260908-5836-forward-admission-BC-gate':'20260908-5906-reduce-root-BC-gate',
    '5836-forward-admission-BC':'5906-reduce-root-BC',
    "('B', 5830, 'release'), ('C', 5831, 'bc')":"('B', 5900, 'release'), ('C', 5901, 'bc')",
    'pin-{number}-forward-admission':'pin-{number}-reduce-root',
    'isolated C2 admission producer5830':'independent Reduce root-guidance cleanup5900',
    'Native clause-addition-only C2 marks preserve expected verdicts and improve cumulative work':'Removing newly root-fixed identities after Reduce preparation preserves verdicts and reduces excess search work',
    '20260908-5837-forward-admission-rank32-prefix':'20260908-5907-reduce-root-rank32-prefix',
    'pin-5832-forward-admission-trace':'pin-5902-reduce-root-trace',
    'tactix-5837-':'tactix-5907-',
    '5837-admission-rank32':'5907-reduce-root-rank32',
    'Remove root-only C2 admission producer;locate earliest changed work and compare with native. Earlier divergences must be reported,not hidden by the179463 witness.':
        'Consume newly derived roots after Reduce preparation before Reorder. Compare every decision against cached native;expect to close225987 but report any earlier or later split. No change to preclean5870 branch or fixed runtime denominator.',
}
code=re.sub('|'.join(re.escape(k) for k in sorted(changes,key=len,reverse=True)),lambda m:changes[m.group()],code)
exec(compile(code,str(source),'exec'),{'__file__':str(source),'__name__':'__main__'})
