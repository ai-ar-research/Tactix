---
name: generate-report
description: Generate an Andrew-facing PDF report from LaTeX on a user-suggested topic or, when wrap-up supplies no topic, the session's main task and current Tactix status. Use for every wrap-up and whenever the user asks for a report, PDF, or write-up under report-to-andrew/.
---

# Generate a report

Create a concise, self-contained deliverable from primary evidence. Do not use
the report as another living status document.

## Workflow

1. Choose the topic in this order: the user's explicit report topic, the
   session's main task, then `Tactix development status`.
2. Synchronize and compact the living docs first. Read the relevant source of
   truth: plan for commitments, PROGRESS for current state, reports/run results
   for evidence, and git for landed changes. Never revive a superseded claim.
3. Create `report-to-andrew/YYYY-MM-DD-HHMM-<topic>/report.tex` from
   `assets/report-template.tex`. Use a lowercase ASCII slug and the local
   America/New_York timestamp; never overwrite an existing report.
4. Replace every template token. Keep the standard structure unless the topic
   clearly needs another: executive summary, findings/results, evidence,
   risks/decisions, and next actions. Cite repository paths, run stamps, report
   names, and commit SHAs close to the claims they support. Escape LaTeX input.
5. Run `scripts/build-report.sh <path/to/report.tex>`. It loads the cluster's
   `amh-tex/2023` module when needed. Fix every compilation error. A missing
   `latexmk` is a blocking prerequisite, not a reason to emit an uncompiled
   deliverable.
6. Confirm both `report.tex` and `report.pdf` exist, the PDF begins with `%PDF-`,
   and no auxiliary files were left in the report directory. Commit both; treat
   them as frozen after delivery.

## Content rules

- Write for Andrew: lead with the conclusion and explain project-specific
  terminology briefly.
- Prefer synthesis and pointers over copied logs or benchmark tables.
- State uncertainty and distinguish measurements from interpretation.
- Do not include credentials, private transcripts, scratch artifacts, or
  uncited performance claims.
- Keep ordinary session reports short; length should follow the topic, not the
  amount of source material available.
