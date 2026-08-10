#!/usr/bin/env python3
"""Enforce DEVELOPMENT_PLAN's no-magic-strategy-number invariant."""
import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
CONFIG = ROOT / "src/policies/default_strategy_config.hpp"
USES = [ROOT / "src/policies/default_policy.cpp",
        ROOT / "src/policies/default_policy.hpp",
        ROOT / "src/policies/default_inprocess.hpp"]


def code_only(text):
    text = re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r'"(?:\\.|[^"\\])*"', '""', text)


def main():
    failures = []
    plan = (ROOT / "DEVELOPMENT_PLAN.md").read_text()
    if "DefaultPolicy contains no magic strategy numbers" not in plan:
        failures.append("DEVELOPMENT_PLAN.md lost the permanent strategy-number rule")
    config = CONFIG.read_text()
    required = ["struct DefaultStrategyConfig", "struct Branching",
                "struct Analysis", "struct Restart", "struct Mode",
                "struct Reduction", "struct Rephase", "struct Reorder",
                "struct Inprocess", "sweep_depth", "sweep_max_depth",
                "sweep_vars", "sweep_max_vars", "sweep_clauses",
                "sweep_max_clauses", "sweep_candidate_pairs",
                "sweep_flip_rounds", "sweep_environment_ticks",
                "serialize() const"]
    for token in required:
        if token not in config:
            failures.append(f"{CONFIG.relative_to(ROOT)} lacks `{token}`")

    # These were the scattered shipped scheduling literals that motivated the
    # rule.  They may occur in the catalog, but not at policy use sites.  Small
    # 0/1 values, result codes, array arities, RNG shifts, and unit conversions
    # are instead covered by named constants or the narrow structural checks
    # below; a lexical checker cannot infer their semantics safely.
    banned = re.compile(
        r"(?<![A-Za-z0-9_])(?:1\.10|1e5|33(?:\.0)?|500|560|1024|2000|300|"
        r"10000|10000000|2000000|700000000|1000000000000)(?![A-Za-z0-9_])")
    for path in USES:
        clean = code_only(path.read_text())
        for match in banned.finditer(clean):
            line = clean.count("\n", 0, match.start()) + 1
            failures.append(f"{path.relative_to(ROOT)}:{line}: strategy literal "
                            f"`{match.group()}` belongs in {CONFIG.name}")

    policy = code_only(USES[0].read_text())
    effort = policy[policy.index("EFFORT_SPECS[]"):]
    effort = effort[:effort.index("\n};")]
    for row in re.finditer(r'\{\s*"(\w+)"\s*,\s*([^,]+)', effort):
        if not re.fullmatch(
                r"&DefaultStrategyConfig::Inprocess::[A-Za-z_]\w*",
                row.group(2).strip()):
            failures.append(f"EFFORT_SPECS `{row.group(1)}` does not name a "
                            "typed strategy-config field")
    for token in ("config_.reduction.initial_interval",
                  "config_.analysis.bump_reason_rate",
                  "config_.rephase.walk_permille",
                  "config().minimum_effort_ticks",
                  "config().preprocess_initial_ticks",
                  "configured_sweep_limits(config())"):
        if token not in policy:
            failures.append(f"policy lost configuration use `{token}`")

    if failures:
        for failure in failures:
            print("FAIL:", failure, file=sys.stderr)
        return 1
    print("default-strategy-config: permanent rule and use sites OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
