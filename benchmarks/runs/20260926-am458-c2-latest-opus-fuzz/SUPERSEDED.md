# AM458 pre-counted stop

Codex stopped Claude Opus owner `10d989fb` on 2026-09-26 before any counted
fuzz leg was submitted. The pinned candidate source is `7b1c8a5a`; current
source includes the separate AM460 proof-only external-literal overflow repair
at `d61295ef`. Uncounted smoke `1727692` and fixed dry run `1727715` were
canceled after 13:44 and 11:15, respectively. Their partial logs and frozen
preparation pins remain local evidence, not accepted fuzz results.

The stopped owner status is a historical preparation snapshot. It must not be
read as an active campaign or acceptance report. The AM457 Sweep repair has
focused independent proof/model/sanitizer checks, while latest-source broad
Opus fuzz remains due after the pending D1/H2 proof repair. No N1--N4 gate
or Tier D promotion follows from this run.
