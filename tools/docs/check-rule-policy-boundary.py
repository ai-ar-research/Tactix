#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
micro = {"analyze", "backjump", "decide", "restart"}
bad = []

for path in sorted((root / "src/rules").glob("*/*")):
    if path.suffix not in {".cpp", ".hpp"} or path.parent.name in micro:
        continue
    text = path.read_text()
    if re.search(r'^\s*#\s*include\s*[<"]policies/', text, re.MULTILINE):
        bad.append(f"{path.relative_to(root)} imports a policy header")
    if re.search(r'\bheuristics_[A-Za-z0-9_]+\s*\(', text):
        bad.append(f"{path.relative_to(root)} calls a heuristic operation")

if bad:
    print("check-rule-policy-boundary:")
    print("\n".join(f"  {line}" for line in bad))
    raise SystemExit(1)

print("check-rule-policy-boundary: macro formula rules are policy-independent")
