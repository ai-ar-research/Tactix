#!/usr/bin/env python3
"""Enforce concise living-document budgets and cheap consistency invariants."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
LIMITS = {
    "CLAUDE.md": 80,
    "DEVELOPMENT_PLAN.md": 750,
    "PROGRESS.md": 350,
    "docs/README.md": 100,
    "docs/calculus.md": 600,
    "docs/implementation-notes.md": 800,
    "docs/lessons.md": 100,
}
SKILL_DEFAULT = 120
# These workflows contain dense operational or diagnostic protocols whose
# detail is not owned by another document. Revisit the exception when editing.
SKILL_EXCEPTIONS = {
    "debug-performance": 130,
    "stress-policy": 140,
    "revisit-codebase": 145,
    "slurm-jobs": 200,
}


def lines(rel):
    return len((ROOT / rel).read_text(encoding="utf-8").splitlines())


def main():
    errors = []
    for rel, limit in LIMITS.items():
        actual = lines(rel)
        if actual > limit:
            errors.append(f"{rel}: {actual} lines exceeds budget {limit}")

    for skill in sorted((ROOT / ".claude/skills").glob("*/SKILL.md")):
        limit = SKILL_EXCEPTIONS.get(skill.parent.name, SKILL_DEFAULT)
        actual = len(skill.read_text(encoding="utf-8").splitlines())
        if actual > limit:
            errors.append(
                f"{skill.relative_to(ROOT)}: {actual} lines exceeds budget {limit}"
            )

    claude = (ROOT / "CLAUDE.md").read_text(encoding="utf-8")
    for invariant in ("N1", "N2", "N3", "N4"):
        if not re.search(rf"\*\*{invariant}\b", claude):
            errors.append(f"CLAUDE.md: missing {invariant} from non-negotiables")

    progress = (ROOT / "PROGRESS.md").read_text(encoding="utf-8")
    for rel in ("DEVELOPMENT_PLAN.md", "PROGRESS.md"):
        text = (ROOT / rel).read_text(encoding="utf-8")
        if re.search(r"\bplan\s+rev(?:ision)?\b|\brev\.\s*\d+\b", text, re.I):
            errors.append(f"{rel}: manual plan revision reference remains")

    states = {}
    for lineno, line in enumerate(progress.splitlines(), 1):
        mark = re.match(r"\s*- \[([ x])\]", line)
        if not mark:
            continue
        for ident in re.findall(r"`((?:SP|SCHED)-[A-Z0-9-]+)`", line):
            state = mark.group(1)
            if ident in states and states[ident][0] != state:
                errors.append(
                    f"PROGRESS.md:{lineno}: {ident} is both open and complete "
                    f"(first at line {states[ident][1]})"
                )
            states.setdefault(ident, (state, lineno))

    recent = progress.partition("## Recent changes")[2]
    recent_count = len(re.findall(r"^- \d{4}-\d{2}-\d{2}", recent, re.M))
    if recent_count > 5:
        errors.append(f"PROGRESS.md: {recent_count} recent changes exceeds limit 5")

    for error in errors:
        print(f"  FAIL  {error}")
    checked = len(LIMITS) + len(list((ROOT / ".claude/skills").glob("*/SKILL.md")))
    print(f"check-doc-size: {len(errors)} error(s), {checked} file budget(s) checked")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
