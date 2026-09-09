#!/usr/bin/env python3
from pathlib import Path
source = Path(__file__).with_name('summarize-retry-full50-6258.py')
code = source.read_text()
for old, new in (('20260909-6239-substitute-retry-ranks1-50', '20260909-6278-vivify-binary-ranks1-50'),
                 ('20260909-6181-learned-sort-ranks1-50', '20260909-6239-substitute-retry-ranks1-50'),
                 ('2026-09-09-substitute-retry-checkpoint-6255.json', '2026-09-09-vivify-binary-checkpoint-6287.json'),
                 ('2026-09-09-retry-full50-6258', '2026-09-09-vivify-full50-6290')):
    assert code.count(old) == 1; code = code.replace(old, new)
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
