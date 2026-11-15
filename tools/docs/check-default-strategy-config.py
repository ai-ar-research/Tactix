#!/usr/bin/env python3
"""Enforce the no-magic-strategy-number source invariant."""
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
    config = CONFIG.read_text()
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

    raw_policy = USES[0].read_text()
    policy = code_only(raw_policy)
    # Pinned Kissat classification has no scheduling consumer. Production
    # policy must not regain a benchmark-, size-, or density-shaped profile.
    for token in ("FormulaProfile", "PROFILE_", "original_external_vars",
                  "original_input_clauses", "kernel_num_output_vars",
                  "kernel_num_original_internal_vars"):
        if token in policy:
            failures.append(
                f"default_policy.cpp contains forbidden formula-profile "
                f"token `{token}`")
    for token in ("Dose5", "Factor5", "NoBve", "NoBveLight",
                  "StartupFactorOnly"):
        if token in policy or token in config:
            failures.append(f"unsupported production profile `{token}` remains")
    # DefaultPolicy has no benchmark identity input. Reject the obvious ways
    # one could smuggle an instance exception into this source in the future.
    if re.search(r'"(?:[^"\\]|\\.)*\.cnf', raw_policy):
        failures.append(
            "default_policy.cpp contains a benchmark-specific CNF literal")
    for token in ('"tierA', '"tierB', '"tierC', '"tierD',
                  "instance_name", "benchmark_identity"):
        if token in raw_policy:
            failures.append(
                f"default_policy.cpp contains benchmark-specific token "
                f"`{token}`")

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
