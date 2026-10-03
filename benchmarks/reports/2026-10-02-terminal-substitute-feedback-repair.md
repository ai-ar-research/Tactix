# Terminal Substitute feedback failure and validated staged repair

Candidate `f6f01982` is **NOT ACCEPTED** for shared execution/feedback preparation.
Claude's delivered [terminal report](2026-10-02-terminal-substitute-feedback-repair/FINAL_REPORT.md)
and [bounded handoff](2026-10-02-terminal-substitute-feedback-repair/FAILURE_HANDOFF.md)
report public callbacks at an unrestored watch boundary. The parent read only
those deliveries and the terminal response, not linked private campaign files.
No verdict, model or proof rejection was reported. The repair below is staged;
root solver sources remain unchanged while full Debug 1746628 owns them.

## Failure and owner disposition

Retained corpus 009, SHA256 `00eba0e110c85125d4f3818ea9fb08239d3db8a2a4c5ffbe6e311b92556786e5`,
with controller seed 2032000128 selects BVE, LocalSearchGuidance and Substitute.
Substitute refutes during its rewrite. Its action callback and session-end
callback see `large_watches_connected == false`. Owner diagnosis 1746782
reproduced this 3/3, including default ranked-prefix retention. DefaultPolicy
had no corresponding violation in the owner's scoped callback sample.

Both fresh legs completed: 100,000 fresh seeds each and 51 corpus entries,
with no reported solver, model, proof or verdict failures. The targeted failure
rejects the overall campaign: R11/R12 fail and other targeted rows remain
partial or unmet. Owner cancelled 1746777/1746778 after the demonstrated failure;
no owner jobs remain live. Source/checker postflight was reported unchanged.

The delivered status distributions sum to 100,010 per leg although the fresh
census is 100,000. Their inclusion of fixed prefix cases is unresolved; do not
use those distributions as fresh-only counts. An attempted owner-context
accounting clarification was rejected by automatic approval review because it
could exceed the delivered-report boundary. No private evidence was accessed.
The next authorized campaign must separate fresh, fixed-prefix and corpus
accounting. The original report and its NOT ACCEPTED disposition remain intact.

## Source diagnosis and proposal

The canonical single-round path in `substitute_apply_round` explicitly skips
watch restoration when a rewrite returns UNSAT. The shared batch executor also
returns UNSAT before its final reconnect. The owner's suggested batch cause
therefore identifies one affected path; direct source review and causal tests
also establish the canonical path used by the reported random sequence.

The staged production proposal removes the single-round terminal exemption and
makes the batch break to its existing final cleanup. Connected-at-entry calls
restore their owned session before public return. An already-disconnected
caller retains ownership. On an inconsistent kernel the propagation service
returns UNSAT before further reasoning, preserving the completed refutation.
This is representation cleanup, with no solver-state rollback, fallback,
scheduling change, new allowance, scalar reward or learning.

The four-file [integration proposal](2026-10-02-terminal-substitute-feedback-repair/integration-proposal/repair.diff)
contains two production edits and regressions in `test_substitute_execution`
and `test_policy_feedback`. Tests include a refutation caused after watch
removal, owned and borrowed single-round/batch calls, and the retained-corpus
controller sequence with both retention settings. The older fixture refutes
without creating a new disconnected session and remains a separate control.

## Staged validation

SLURM 1746784 completed 0:0 in 21 seconds on 2 CPUs/8192 MiB. Original versus
patched execution gives the expected outcomes in all eight ownership/batch
cases. The exact reported sequence runs with full/ranked-prefix retention and
feedback off/on: original code exposes the defect; patched code has no callback
violation and finishes connected. All eight emitted proofs are independently
checked by canonical drat-trim/CakeLPR routing against the original CNF. Their
bytes are identical (one distinct certificate, SHA256
`6603c358619c4e6862273b1abbdc17ab7c05040b73a2dacf70df386aa14f6e60`).
These are eight repeated oracle checks, not eight independent formulas.

SLURM 1746788 completed 0:0 in 20 seconds with the same resources. Both new
regressions reject the original implementation at the watch-boundary assertion.
All seven staged focused tests pass: substitute_execution, policy_feedback,
substitute, substitute_publication, native_policy_feedback,
default_execution_signature and inprocess_execution_signature. The last two
are existing assertion/signature fixtures; this run is not a new complete
preserved-source comparison or performance measurement.

Both jobs verify the active 984 bindings before/after and their own staged
manifests. The standalone harness's first compile failed because it omitted a
required interval callback and used a nonexistent Proof close method; the API
usage was corrected before submission, and that failed build is retained.
Neither production proposal nor regression sources have been applied to root.

## Remaining acceptance

Full Debug 1746628 is still active on `f6f01982`; preserve its 795 sources until
terminal exact JUnit/exit and 984 postflight complete. The current pin's
[four default regressions](2026-10-02-shared-policy-repaired-regressions.md)
and [feedback measurement](2026-10-02-shared-policy-repair-revalidation-started.md)
remain valid within that pin's scope and do not accept this proposal.

After the active source owner finishes, apply the validated four-file repair,
update its owning interface/engineering notes if needed, freeze new Debug and
Release pins, and obtain fresh full Debug, both 100,000-seed legs plus corpus,
complete targeted/mining acceptance, all four default gates and feedback
purity/overhead evidence. Then audit the original objective and finish the final
Andrew handoff. All frozen evidence and unrelated work are preserved; no cleanup
or push occurred. O6 reward design remains deferred.
