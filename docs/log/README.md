# PROGRESS Log archives

`PROGRESS.md`'s Log keeps only the **current ISO week**. Completed weeks are split out here,
**verbatim and frozen** — an archive file is never edited after it lands.

**Maintenance contract (enforced at wrap-up):** when the ISO week rolls over, or whenever
`PROGRESS.md` exceeds its size budget, move every entry older than the current week into
`docs/log/<ISO-week>.md` and add its row below. Never summarise on the way out — the Log is the
durable record; compression happens *before* an entry is written, not after.

| week | dates | entries | what it covers |
|---|---|---|---|
| [2026-W32](2026-W32.md) | 08-03 → 08-09 | *(current — lives in PROGRESS.md)* | broad-Factor re-test, the N2 LRAT fix, plan rev 80 / N4 |
| [2026-W31](2026-W31.md) | 07-27 → 08-02 | 41 | all-rules-ON, TB4b, task #11, R-d retirement, L19–L23 |
| [2026-W30](2026-W30.md) | 07-20 → 07-26 | 31 | E2 Factor, V6 Congruence, the S4 arrangement, Track W Walk |
| [2026-W29](2026-W29.md) | 07-13 → 07-19 | 2 | phase-2 close, the inprocessing revamp |
| [2026-W28](2026-W28.md) | 07-06 → 07-12 | 2 | phases 0–1, T1 |

**Reading an archive.** Lesson numbers (`L<n>`) and the four `R<n>` namespaces are defined in
`PROGRESS.md` and are **never renumbered**, so an archived entry's citations stay resolvable. Numbers
quoted inside an archived entry are the numbers *as measured then* — later corrections live in
`PROGRESS.md` (standing lessons + the current tier tables), which is authoritative on conflict.
