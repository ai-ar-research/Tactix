import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import tempfile

root = Path(__file__).resolve().parents[1]
path = root / 'tools/docs/check-effort-specs.py'
spec = importlib.util.spec_from_file_location('effort_checker', path)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)
source = Path(checker.POLICY).read_text()
normalized = checker.squash(checker.strip_comments(source))
pattern = checker.EV_SR_ROOT_OWNER
matches = re.findall(pattern, normalized)
assert len(matches) >= 2, matches
for call in ['run_structured_root(k, stop)', 'run_structured_root(k, stop, completion)',
             'run_structured_root(k, stop, completion_)']:
    assert re.fullmatch(pattern, call), call
for call in ['run_structured_root(other, stop, completion_)',
             'run_structured_root(k, other, completion_)',
             'run_structured_root(k, stop, unrelated)', 'run_structured_root_missing(k, stop)']:
    assert not re.fullmatch(pattern, call), call
mutated, removed = re.subn(pattern, 'removed_root_owner()', source)
assert removed == len(matches)
with tempfile.TemporaryDirectory(prefix='tactix-governor-control-') as directory:
    negative = Path(directory) / 'default_policy.cpp'
    negative.write_text(mutated)
    checker.POLICY = str(negative)
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        status = checker.main()
assert status == 1 and 'UNGOVERNED: `SubstitutionRedundancy`' in output.getvalue()
data = {'checker_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'policy_source_sha256': hashlib.sha256(source.encode()).hexdigest(),
        'root_owner_calls_removed': removed, 'valid_signature_controls': 3,
        'invalid_signature_controls': 4, 'missing_governor_rejected': True,
        'negative_output': output.getvalue()}
(root / 'build/shared-policy-prep-startup-support-governor-controls.json').write_text(json.dumps(data, indent=2) + '\n')
print(f'Governor controls: {removed} current calls matched; 3 valid and 4 invalid signatures; removed root owner rejected.')
