# Watch-rebuild cost candidate and stronger LRAT oracle

Candidate6902 is an isolated, unpromoted/unfuzzed cost experiment on6730.
It skips redundant word scans of binary-only watch lists while preserving the
old metadata effect. No runtime gain is claimed yet. A new negative control
also exposed false acceptance by the legacy C LRAT checker; the new fixed gate
uses the existing verified cake_lpr binary instead. Production is unchanged;
full50 remains8300.164/7239.010=1.146588277, above the1.01 target.

## Cost hypothesis and exact state

[Annotation6900](../runs/20260910-6900-watch-rebuild-annotation/summary.json)
reads the existing checked6730 Rank32 profile, not a new solver run. Rebuild
has1413 self samples and26713102690 sampled periods. The complete function
has2.15% self/3.39% inclusive samples. [Location6901](../runs/20260910-6900-watch-rebuild-annotation/location-6901.json)
attributes4012463716 periods to the hot compaction loop:15.020583% of function
and0.322787% of program. This excludes cold compaction blocks and includes
useful work on connected mixed lists; it is not a removable-cost ceiling.

Vivify/Substitute disconnect large watches before reconnecting. Reconnect
nevertheless scans all watch words again to retain binaries. Kissat's reconnect
traverses live arena clauses and appends their watches without that second
binary-only pass. CaDiCaL has distinct binary/large clause passes, not a neutral
replacement. Existing dense-order scratch and in-place replacement stay intact.

6902 uses the existing exact per-list binary population: if no large entries
are present, it normalizes the conservative binary-first flag directly. Empty
lists require this too. A bare skip would not preserve that effect. Mixed lists
retain the old compaction, and the complete arena-order/reselection/reconnect
suffix is unchanged. No global disconnected-state assumption, new field,
representation, reference packing, guard, proof vocabulary, policy, budget,
effort currency or CDCL behavior is changed.

[Control6904](../runs/20260910-6902-watch-rebuild-build/control-6904.json)
compares actual old/new kernel objects; all shared objects have identical
source/headers. Both arms pass144 complete-image cases, digest6d4a6f3bd2f702ef,
the full kernel suite and840 replacement-cursor cases. Tests cover dead/live
watch storage, heads, holes/capacity, clause arena, ticks, roots, connected/drop/
flush entry, both cleanup flags, attached ordered dense views, mixed/binary/
empty lists and conservative order bits. Connected re-entry is idempotent.
The first6903 fixture link failed on a misspelled constructor before any fixture
or solver execution; failed logs/object remain, corrected6904 passes.

[Build6905](../runs/20260910-6902-watch-rebuild-build/manifest.json) has independent
113/113/112 passing full suites. Release and B/C SHA256 are
ee826004158f80335f98862c05810fd06dbe5a9396709252d35275ee73f260a1;
trace is8f968346095a68e021b0d4801d7e19a2ba8ca7991dfceb6cc8a0e30fe3eb972d.
Source differs from6730 only in kernel.cpp and the new test_watch_rebuild.cpp.
The rule/policy boundary check passes; no calculus/guard change is needed.

## Checker failure: not a solver failure

6906 deliberately supplies satisfiable CNF `p cnf 1 1; 1 0` and invalid LRAT
`2 0 1 0`. Pinned lrat-check returns0 and `c VERIFIED`. No solver emitted it.
DRAT-trim rejects the equivalent negative. cake_lpr rejects the LRAT with
`pivot must be non-zero`, despite returning0. Exit code alone is insufficient.

[Classification6911](../runs/20260910-6902-watch-rebuild-build/scope-6911.json)
binds the reproducer, checker binaries and pinned drat-trim source. In
checkClause, an empty lemma produces a zero pivot. After its hints reach no
conflict, the RATs==0 path can return SUCCESS when the opposite-pivot introduction
slot is zero. The negative test reproduces this unsound acceptance path. Do not
use this legacy checker alone as an acceptance oracle. No reference clone,
checker binary, frozen harness, campaign or historical report was modified.

Historical lrat-check-only success does not independently establish LRAT
acceptance after this counterexample; those historical proofs are not thereby
proved invalid. The current/recent large runtime and B/C jobs use DRAT-trim,
not this legacy checker. Their checks can continue. Retained6730 eight root/
reason witnesses were rechecked without mutation; all pass, including four
LRAT witnesses now independently checked by cake_lpr. No routine solver or
fixed-reference rerun is implied.

[Fresh6910](../runs/20260910-6910-watch-rebuild-proofs/summary.json) checks128
reconnection-dependent fixed proofs,64 per actual source arm, with exact input,
certificate bytes and state outputs. Both modes, root timing, cleanup flags,
watch orientation and attached dense views are covered. M1 emits each final
refutation through a reconnected large antecedent. Native LRAT uses cake_lpr;
DRAT uses DRAT-trim. Both SAT negative controls reject. New maintained
proof_oracle.py has eight status tests; two binary integration tests continually
exercise the actual positive and negative fixtures. No import or dependency on
fuzz artifacts is required. All262 benchmark-tool tests pass.

## Runtime gate and handoff

B28+C5 job1679218 was submitted once at03:07:03EDT under the unchanged full-width
12CPU/48000MiB/1200solver-second, independent16000checker-second/5h contract.
Watcher25232 owns observation. The initial positional watcher invocation failed
argument parsing only; the corrected --job watcher did not resubmit the solver.
Strict6909 now owns all66 expected solver outcomes and33 exact work/certificate
pairs,64 checked/clean at its snapshot. Wave0/job1679284 was submitted once at
03:14:53EDT, watcher27451; remaining waves only after exact preceding solver
outcomes, with proofs asynchronous. The three
waves retain32/39/38, reversed middle order, exact6730 work/proof oracles and
complete runtime/RSS/phase-cost pricing through6914/6915. No best-of repeats,
packing, new representation or adoption inferred. PROGRESS owns current jobs.

Audit6917 binds current published states, active jobs, prior immutable evidence,
new files, deadline and aggregate checks. Annotation6900 removed557169956bytes
of temporary expansion, retaining the compressed original/raw annotation.
Failed6906 removed16bytes of its completed DRAT negative before its LRAT failure;
the22-byte LRAT counterexample stays as unique evidence.6910 removed7366bytes
of verified fixed inputs/proofs with logs/hashes/generator retained. No live
checker scratch was removed. An overly broad read-only checker-reference search
incidentally returned three excluded archived fuzz metadata hits; it was stopped,
and none is used as acceptance evidence or a dependency. No further access.

Andrew's report is local/unsent. Docs stay within existing caps. Source fuzz,
memory acceptance, full50 convergence and global scheduling/CDCL parity remain
open. Preserve4/32/33/49/46, N1–N4 and the stronger bounded true-level scan.
Rank21 remains shelved; no FlatLists, promotion, denied transfer/push retry or
fixed Kissat refresh. Winddown05:45/no-new-task06:00EDT September10 remains.
