# Shared-policy fuzz acceptance corrected; oracle closeout required

Whole-goal acceptance remains **pending** on production source `375b621b`.
The Claude owner delivered an ACCEPTED report, then corrected its disposition
to **NOT ACCEPTED as delivered** in a public addendum. The correction identifies
unchecked semantic completions and incomplete terminal provenance. It reports
no observed solver, proof, model or checker failure. Both delivered reports
remain frozen, including the original overstatement.

The [hashed companion](2026-10-03-shared-policy-fuzz-closeout-required/validation.json)
preserves only delivered public owner material and parent-owned review evidence;
the parent has not inspected private campaign scripts, inputs, results or logs.

## Established evidence and its limits

The owner reports 100,000 unique fresh cases per policy, with every completed
fresh result independently checked. Fixed-prefix controls, corpus and mining
replays earn no fresh credit.

| Fresh policy | SAT | UNSAT | Solver timeout | Checker timeout/rejection/error | Unknown/crash |
|---|---:|---:|---:|---:|---:|
| DefaultPolicy | 57,423 | 39,901 | 2,676 | 0 | 0 |
| RandomControllerPolicy v1 | 56,018 | 38,302 | 5,680 | 0 | 0 |

The separate 51-entry corpus replays have no reported failure. Default corpus
022 retains its declared crash-oracle-only exception. All R1–R13 coverage rows
and the terminal-watch row are reported met on independently checked evidence:
25 effective action families, all 20 configurations, repeated root actions,
both-policy partial/full restarts, continuation/staleness, interruption,
observation/feedback purity, replay and causal negative controls. These coverage
results do not close the remaining all-completions/provenance clauses.

The repaired Substitute boundary is exercised by retained corpus 009 under
both retention settings and by genuine post-disconnection single/batch
refutations with owned and borrowed scopes. Predecessor controls still detect
the owned-watch defect. Factor's unreachable terminal arm remains explicitly
dispositioned; it is not forced using an invalid kernel state.

## Why acceptance is withheld

- **C1, mining scan:** 20,000 replay runs produced 11,382 SAT exits, 7,702 UNSAT
  exits and 916 solver timeouts. The 19,084 completions were not independently
  checked. Although 18,888 have graded fresh-leg counterparts, 196 have none;
  counterpart verdicts alone cannot verify a mining certificate/model. The
  separate 42 mining follow-up rows were graded and support their coverage
  claims, but do not disposition the whole scan.
- **C2, provenance:** the owner's last pin/checker postflight preceded the end
  of diagnosis d1 and the entire d2 job. Neither diagnosis performed in-job
  pin/checker/snapshot verification. A later check must retain this historical
  gap rather than invent missing checks.
- **C3, raw diagnosis completions:** d2 has six raw UNSAT completions and
  three independently graded reruns. Raw off/on proofs match each other;
  independent grading or demonstrated correspondence to a checked artifact
  still needs an explicit disposition for the raw completions.

All main owner jobs and diagnoses are terminal. The original CLI `3818` and
clarification CLI `32432` are closed. The same exclusive owner now handles the
bounded oracle/provenance closeout through CLI `39538`, using the
[parent request](../runs/20261002-shared-policy-feedback-375b621b-preparation/fuzz-owner-closeout-request.txt).
It permits retained-artifact checking or exact graded replay where artifacts
are absent, with complete old/new accounting and no fresh-seed credit. No solver
change, additional benchmark campaign or overhead optimization is requested.
The owner must stop on an oracle/solver failure and deliver a bounded handoff.

## Feedback cost and native distinctions

The addendum explains all targeted timeout categories as solver-process
deadlines, not checker deadlines. Six DefaultPolicy recording-variant timeout
rows remain non-evidence: four CLI stop-fuzz rows were diagnosed; two TF rows
were not. d1 completes and checks gen-25 n50/n500. d2 completes gen-25 n50 and
gen-15 n50/n500 with equal deterministic counters and raw off/on proof bytes.
The three d2 configurations show approximately 68–88 times Debug CLI recording
slowdown, including one stream of about 50 GB. This is a material limitation,
not negligible overhead or a general Release-performance claim.

The owner's root-only assertion applies to controller root choices and other
root-only families. DefaultPolicy's native level-independent Propagate, Reduce
and Reorder retain their boundaries. R11 validates schema and lifecycle fields,
executor correspondence for scoped requests, and duration presence; it does not
independently remeasure every duration or native effort value. Source inspection
and focused metering controls remain separate evidence.

## Parent gates and next action

[Full Debug](2026-10-03-shared-policy-full-debug-accepted.md) passes exact 212/212.
[All four selected default gates](2026-10-03-shared-policy-regressions-recovered.md)
remain accepted with explicit checker censoring and the single registered WV100
recovery; the original failed WV100 run stays rejected. The scoped paired
frontend measurement remains accepted, adding median 17.597–63.084 ms on four
small synthetic inputs. Neither measurement establishes cheap large-instance
feedback. No additional default regression retry is queued.

After the original owner jobs ended, the parent reverified all 984 current
binary/source/support bindings, 14 frozen Debug companion hashes, 31 frozen
regression companion hashes, 30 reviewed operation-map source hashes, twelve
source/contract hashes and seventeen skill hashes. The solver-source diff from
`375b621b` to current HEAD is empty. This parent check does not replace the
owner's outstanding checker/snapshot postflight.

Next obtain the delivered owner closeout, audit every original requirement at
its proper scope, reconcile living documentation and complete Andrew's final
TeX/PDF handoff. Reward choices and learning remain deferred. No fallback,
rollback, forced ResumeSearch, global loop budget, private parent inspection,
unrelated cancellation, deletion or push is introduced.
