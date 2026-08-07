# Documentation map

The single place that answers **what docs exist and which ones must be kept
current.** The maintenance burden is small — only the *living* docs need syncing;
everything else is a frozen, dated artifact.

## What exists, and what you maintain

| Tier | Files | Keep current? | Role |
|---|---|---|---|
| **Living** | `DEVELOPMENT_PLAN.md`, `PROGRESS.md`, `docs/calculus.md`, `docs/implementation-notes.md` | **Yes — these four** | the map + the truth |
| **Living (reference)** | `docs/lessons.md` | **Yes** | measured lessons, freely revisable. **Never cite one by number** — state the finding and its primary evidence instead |
| **Contract** | `CLAUDE.md`, `README.md` (repo root) | Rarely (process/pitch change) | standing rules for Claude; elevator pitch for humans |
| **Design snapshots** | `docs/design/*.md` | **No — frozen on landing** | how a feature was designed / what a reference solver does |
| **Reports** | `benchmarks/reports/*.md` | **No — immutable evidence** | dated experiment & gate results |
| **Log archive** | `docs/log/` | **No — frozen weekly** | PROGRESS Log entries archived by ISO week; has its own README + maintenance contract |
| **Resources** | `docs/papers/` | — | external papers |

The four **core living** docs, and what each owns (see protocol 1) — `docs/lessons.md` is living too,
but it is a working aid rather than an authority and owns no fact of its own:

- **`DEVELOPMENT_PLAN.md`** — scope, architecture, roadmap (requirements,
  non-negotiables N1–N4, rule catalog §5.2, tiers/gates §8, milestones §9, open
  decisions §11). *The map.* Changes go through the **revise-plan** skill.
- **`PROGRESS.md`** — live checklist + newest-first log. *"Where are we now."*
  Synced with the plan in the same commit (mandatory).
- **`docs/calculus.md`** — the abstract proof rules (precondition / effect /
  proof-δ / soundness). Maintained via the **calculus** skill.
- **`docs/implementation-notes.md`** — the engineering layer paired with the
  calculus: implemented guards, the deviation ledger (every guard stronger than
  its precondition, with a removal path), and standalone design decisions.

## The three protocols that keep this from sprawling

1. **One fact, one home.** Every fact has exactly one authoritative doc; all
   others link, never restate. Rule semantics → `calculus.md`. Guards/deviations →
   `implementation-notes.md`. Scope/architecture → the plan. Status → PROGRESS.
   Evidence → a report. Restating a fact that lives elsewhere is the smell to
   avoid.

2. **The plan is the index — nothing is loose.** Every design doc and report is
   reachable by a one-line pointer from the plan (§13 / the catalog) or PROGRESS
   (the log). You never have to *remember* a doc exists; you find it from the map.

3. **Design docs and reports freeze on completion.** The moment a feature ships,
   its design doc carries a "Frozen snapshot — do not edit" header, lives in
   `docs/design/`, and is never touched again; its living truth is the plan +
   calculus. Reports are immutable by nature. So `docs/` top-level holds only the
   living docs (`calculus.md`, `implementation-notes.md`, `lessons.md`), this
   map, and the frozen `design/` + `log/` + `papers/` folders.
   **Reading a frozen doc:** it is a record of what was believed when written.
   In particular everything under `design/` and `log/` predates plan rev 83, so
   its phase numbers are the old ones (old 3.5→4.5, 4→5, 5→6, 6→7, and "phase 3"
   there means the pre-split phase that still owned dosing). Read a phase number
   against the doc's date, never against the current plan §9 table.

**Before creating a new doc:** prefer extending a living one. A new doc is
justified only for (a) a design pass *before* a large feature (→ `docs/design/`)
or (b) a dated experiment/gate report (→ `benchmarks/reports/`) — both frozen once
written. (Also stated in `CLAUDE.md`.)

**Keeping the living docs healthy:** the **compact-docs** skill compacts them (and
enforces these protocols) — condense completed-work detail to a result + pointer,
remove superseded content, dedupe facts to their one home — while guaranteeing a
fresh session still finds the big picture and the undone steps. Run it at phase
gates, after a big feature lands, or when a living doc grows unwieldy.
