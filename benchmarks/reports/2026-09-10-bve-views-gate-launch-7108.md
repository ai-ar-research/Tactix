# BVE views: exact B/C solver gate and runtime launch

Stage1 candidate7087 remains byte-identical to its original source/pins.
The direct fixture repair7100 and strong proof controls7101 cleared the prior
validation failures without changing solver code. Successor7103 uses those
dependencies and preserves the full B28+C5 membership, runner and resources
from7034; the obsolete unexecuted7091 helper is not reused as gate authority.

## Full B28+C5

Job1679843 submitted66 one-solver tasks on cpu-dense-hwulab-q,12 logical CPUs,
48000 MiB,1200 solver-only seconds and16000 checker seconds, with5h wall time,
no throttle or node constraint. Current7030 and candidate7087 have independent
Release/B/C pins with matching within-source executable hashes.

[7104](../runs/20260910-7103-bve-views-BC-gate/neutrality-7104.json) seals66
expected solver outcomes and33 exact pairs: all reported non-timing statistics
and certificate fingerprints agree. Its snapshot has64 checked/clean records
and two pending UNSAT checks. This is not full hidden-state identity or proof
that SAT model bytes are identical; original-formula model checks are separate.
Pending checks permit provisional continuation, not acceptance or promotion.
The first premature seal attempt stopped before writing because only64 solver
results existed; after all66 appeared, the one successful seal was written.
No solver failure, exclusion or gate relaxation occurred.

## Repeated runtime

[7105](../runs/20260910-7105-bve-views-runtime/manifest.json) predeclares18
one-solver tasks: three sequential six-task paired waves on ranks31,35,38.
The first wave, tasks0–5, was submitted as1679910 at14:15:53 EDT while the last
B/C checkers continued. The middle wave reverses arm order. Same resources,
immutable run-local pins, node-local scratch and cleanup contract; no Kissat
rerun, frozen-denominator change or interference with earlier campaigns.

Both arms must match the fixed7030 non-timing work/certificate fingerprint,
including exact own repetition. Advance waves only after their six expected
solver outcomes and those checks; do not wait for proof verification.
Any invalid proof/model or abnormal termination pauses affected work.
The summary retains all three ratios, median/range, within-arm spread, RSS,
placement, printed BVE/slot/outside timers and complete first-wave statistics.
The printed BVE timer is not the complete Eliminate lifecycle timer. Absolute
runtime claims require at most2% within-arm spread. No repeat-to-select.

No timing result or speedup is claimed at launch. The20% complete-Eliminate
target applies to the approved multi-stage prototype, not this initial view
change alone. Further mapping/index preparation work remains approved but is
not implemented in this checkpoint. Production and scheduling are unchanged;
current-source fuzz and cumulative full50 remain adoption requirements.

## Tests, hygiene and next action

The strict pair comparator retains its timing-only positive and eleven
negative fixtures. The wave fingerprint retains timing exclusion, five-change
and three-invalid-schema tests. One new maintained regression covers the
18-task, three-rank BVE cost matrix and reversed middle wave; all14 timing
parser/aggregation tests pass. No runner or solver behavior changed.

Use submit-bve-views-wave-7106.py with wave1 and then2 after the preceding
solver results are published; summarize with summarize-bve-views-waves-7107.py.
Monitor exact submitted jobs via tools/slurm/wait.py and recheck published
proof/model outcomes at checkpoints. Frozen pins, small fixed reproduction
artifacts and active checker scratch are preserved; no material deletion,
fuzz access, source promotion or denied push retry.7108 owns the checkpoint
audit and Andrew's local report; prior reports and campaigns remain immutable.
