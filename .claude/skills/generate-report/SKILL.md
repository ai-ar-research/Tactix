---
name: generate-report
description: Generate the mandatory Andrew-facing LaTeX and PDF report at session wrap-up or on explicit request. Summarize verified current state, decisions, evidence, risks, and next actions without forcing documentation compaction.
---

# Generate Andrew's report

Read the plan, PROGRESS, relevant diffs, test results, benchmark reports, and
primary evidence. Do not invoke `compact-docs` merely to prepare a report.

Create a new timestamped directory under `report-to-andrew/` containing the TeX
source and compiled PDF. Future report directories are local/untracked; the four
historical tracked TeX/PDF pairs are frozen and must not be edited.

The report should be as long as the topic warrants and should state:

- objective and outcome;
- material implementation or design decisions and why;
- correctness and performance evidence with exact provenance;
- limitations, failed checks, and unresolved risks; and
- the next concrete actions.

Distinguish measured facts from inference. Link or cite repository artifacts
instead of copying long logs. Compile the PDF, inspect the compiler result, and
verify both files exist; compilation failure blocks completion. Use
`scripts/build-report.sh path/to/report.tex` so compilation is isolated and the
cluster TeX module is loaded when available. Start new reports from
`assets/report-template.tex` when useful.
