#!/usr/bin/env python3
"""Enforce concise living-document budgets and cheap consistency invariants."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
LIMITS = {
    "README.md": 80,
    "CLAUDE.md": 80,
    "DEVELOPMENT_PLAN.md": 750,
    # Dedicated living workstream keeps the main plan within its existing cap;
    # this budget covers objective, architecture, verification and open decisions.
    "docs/winner-integration-plan.md": 200,
    "PROGRESS.md": 350,
    "docs/README.md": 100,
    # Raised 2026-08-12 for the same reason the split below records: calculus
    # sat at exactly 600 and implementation-notes near 520, so both read green
    # while the next semantics or disposition could not be written at all.
    "docs/calculus.md": 680,
    # Split 2026-08-11: section 13 became the parity ledger below. Both caps
    # carry reserve on purpose — the old 800 was saturated at exactly 800, so
    # it read green while the next invariant could not be written at all.
    "docs/implementation-notes.md": 560,
    "docs/scheduling-parity-ledger.md": 600,
    "docs/lessons.md": 100,
    "docs/design/phase-4.20-proof-rule-survey.md": 180,
    "docs/design/phase-4.20-solving-strategy-survey.md": 180,
    "docs/proof-rules.html": 1000,
    "tools/fuzz/README.md": 80,
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
    warnings = []
    for rel, limit in LIMITS.items():
        actual = lines(rel)
        if actual > limit:
            errors.append(f"{rel}: {actual} lines exceeds budget {limit}")
        elif actual > (limit * 9) // 10:
            # A budget with no reserve fails only when someone is already
            # mid-edit, which forces an unrelated trim to land the real change.
            warnings.append(
                f"{rel}: {actual} of {limit} lines — under 10% reserve left, "
                f"compact it before the next addition needs the room")

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

    for warning in warnings:
        print(f"  warn  {warning}")
    for error in errors:
        print(f"  FAIL  {error}")
    checked = len(LIMITS) + len(list((ROOT / ".claude/skills").glob("*/SKILL.md")))
    print(f"check-doc-size: {len(errors)} error(s), {len(warnings)} warning(s), "
          f"{checked} file budget(s) checked")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
