# Late root-reuse checkpoint findings

This supplements the frozen root-conflict-reuse report and its13:50 audit.
Production and submitted sources remain unchanged; the original runtime goal
is not achieved and no promotion is authorized.

## Rank31 localization and completed timing checks

[5735 summary](../runs/20260908-5735-root-reuse-rank31-prefix/summary.json)
validates two clean300K stops, immutable inputs/pins, complete decision streams
and cleanup. There are **76,952,511 matching from-zero decision rows**; the first
split is conflict179823, decision76834806, level41: Tactix−39842 versus
Kissat−32092. This independently extends the current-source investigation; it
does not assume the historical5459 prefix. Localize the incoming score/formula
state before treating the changed branch choice as a defect.

[5679 final summary](../runs/20260908-5679-assignment-level-rank39-runtime/summary.json)
now closes6/6 checked proofs, with the same exact-work median1.03047751x and
6.01% candidate spread. Checking completion does not turn that noisy measurement
into a demonstrated gain. Latest full5712 had65/80 checks and5678 had65/66 at
the preceding audit; their remaining checkers continue independently.

## Public-target obligation discovered in final review

N1 and the existing M3 action surface expose a backjump target. Both the rule's
calculus and its source comment specify caller selection of any legal level,
with canonical `jump_level`. However, `conflictanalysis_apply_instance` invokes
`analyze_apply` before interpreting the requested instance, and returns early
when reuse has already consumed the conflict. Reuse has already selected the
DefaultPolicy-style chronological target. The comment that there is “no target
to honour” does not establish the public contract.

This applies to nonzero reuse in the predecessor as well as the new root path.
The5718 fixed fixture makes a concrete counterexample visible: depth103,
reuse enabled, explicit target0 returns level102 even though jump0 is legal.
The fixture currently **expects**102; passing its enumeration and dispatch tests
does not establish that a requested backjump level was honored. Thus the root
report's API coverage must not be read as closing N1. This is a public-action
semantics issue, not a rejected proof, unsound verdict, crash or sanitizer failure.

Next implementation must use a new isolated successor, leaving5718 and all
submitted pins unchanged. Thread caller target choice into the reuse transition
before backtracking, keep DefaultPolicy's faithful native target unchanged, and
make canonical/explicit public calls honor the documented clamped target. Do
not fix this by removing offered instances or documenting a weaker capability.
Required new tests distinguish internal/default-policy dispatch from canonical
and explicit public targets, including root/nonroot reuse, chronology/clamping,
OTFS reuse, heuristics modes and independent DRAT/LRAT micro-proofs. Preserve
general soundness, reason and reconstruction coverage and verify unchanged
DefaultPolicy work before attributing any timing effect.

No new solver edit occurs in this supplemental review. Existing concrete fixture
execution and static dispatch inspection establish the discrepancy; new unit
tests are explicitly required for its repair. Code/docs diff checking passes
when excluding frozen raw logs and patch artifacts, whose original whitespace
must remain intact. No material manual deletion or push occurred.
