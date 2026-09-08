#!/usr/bin/env python3
"""Validate changed-selector history with all other6020 artifacts unchanged."""
from pathlib import Path
source = Path(__file__).with_name('validate-clause-pose-series-6009.py')
code = source.read_text()
code = code.replace('20260908-6008-clause-pose-series', '20260908-6024-vivify-lemma-history')
code = code.replace('{-246596, -5409, -47, 49, 5415, 148944, 238853}', '{53, 51, -47, -93630, -4166, -4349, 4172}')
code = code.replace("'bve', 'sparsecollect'}", "'bve', 'sparsecollect', 'reduce'}")
begin = code.index("        binding = m['cached_controls'][arm]")
end = code.index("        for name, digest in r['trace_sha256'].items():", begin)
code = code[:begin] + """        parentp = ROOT / m['parent_campaign'] / 'results' / (arm + '.json')
        parent = json.loads(parentp.read_text())
        assert r['partial_certificate'] == parent['partial_certificate']
        assert set(r['trace_sha256']) == set(parent['trace_sha256'])
        for name, digest in parent['trace_sha256'].items():
            if name != arm + '.clause-pose-series.tsv': assert r['trace_sha256'][name] == digest, name
""" + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
