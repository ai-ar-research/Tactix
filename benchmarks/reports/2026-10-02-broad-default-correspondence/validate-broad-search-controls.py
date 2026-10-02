import json
from pathlib import Path
import sys

root = next(p for p in Path(__file__).resolve().parents if (p / 'CLAUDE.md').is_file())
build = root / sys.argv[1]
controls = {}
for profile in (5, 6):
    path = build / f'broad-search-correspondence/enabled/profile-{profile}-pigeons-6.feedback.jsonl'
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    actions = [row for row in rows if row['type'] == 'action']
    if profile == 5:
        selected = [row for row in actions if row['identity'] == 'Rephase']
        identities = sorted(set(row['instance'] for row in selected))
        assert identities == [0, 1, 2], identities
        controls['rephase'] = {'calls': len(selected), 'instances': identities}
    else:
        selected = [row for row in actions if row['identity'] == 'LocalSearchGuidance']
        effective = [row for row in selected if
            row['after']['native']['local_search_hints_applied'] >
            row['before']['native']['local_search_hints_applied']]
        assert effective and len(effective) == len(selected)
        controls['local_guidance'] = {'calls': len(selected), 'effective_receipts': len(effective),
            'hint_source': 'Embedding-supplied Parity advisory assignment; not a model-producer reach claim.'}
(build / 'broad-search-controls.json').write_text(json.dumps(controls, indent=2) + '\n')
comparison_path = build / 'broad-search-correspondence.json'
comparison = json.loads(comparison_path.read_text())
comparison['explicit_search_controls'] = controls
comparison_path.write_text(json.dumps(comparison, indent=2) + '\n')
print(json.dumps(controls, sort_keys=True))
