#!/usr/bin/env python3
"""Build-only diagnostic control: corrected guidance, legacy physical roots."""
import json
from pathlib import Path
import shutil
import sys
root=Path(__file__).resolve().parents[3];sys.path.insert(0,str(root/'tools/bench'))
from fixed_runtime import sha
authority=root/'benchmarks/runs/20260911-7373-lucky-reattach-build/manifest.json'
m=json.loads(authority.read_text());parent=root/m['source']
source=root/'.claude/worktrees/runtime-corrected-legacy-7378';c=root/'benchmarks/runs/20260911-7378-corrected-legacy-prepare'
assert not source.exists() and not c.exists()
for rel,digest in m['files'].items():assert sha(parent/rel)==digest
source.mkdir();c.mkdir()
for name in ('src','include','tests','examples'):shutil.copytree(parent/name,source/name)
shutil.copy2(parent/'CMakeLists.txt',source/'CMakeLists.txt')
for name in ('tools','benchmarks','DEVELOPMENT_PLAN.md'):(source/name).symlink_to((parent/name).readlink())
(source/'build').mkdir()
with (c/'manifest.json').open('x') as out:json.dump(dict(parent_manifest=str(authority.relative_to(root)),parent_sha256=sha(authority),parent_files=m['files'],source=str(source.relative_to(root)),helper_sha256=sha(Path(__file__)),scope='Diagnostic only: turn off root folding at the single existing DefaultPolicy Reduce call, retaining both repaired guidance lifecycles. Public compaction folding API and every other code path unchanged. Require complete proof and non-storage work parity with corrected folded7373 on B14 before Rank25/31 attribution.'),out,indent=2)
print(source)
