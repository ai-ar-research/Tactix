# Canonical Substitute removal publication (isolated6231)

## Implemented transition

The actual6237 Rank32 replay restores the two missing337674 retry marks at
319386/73705568. The general repair queues every disappearing canonical binary
copy (tautology or unit image), then publishes BVE retry events and BCE removal
epochs after the binary-unit batch reaches root closure. Fixed endpoints are
skipped at that boundary. Surviving rewrites do not count as removal; connected
Sweep keeps its existing path. DRAT streaming and canonical LRAT/image paths
share the publication boundary. No guard, public instance, proof serialization,
reconstruction scheme, policy threshold, or benchmark selector changed.

Kissat's `substitute_binaries` queues delayed deletions and calls its ordinary
binary-deletion primitive after unit propagation; its removed-literal helper
skips fixed variables. CaDiCaL decomposition garbage-marks disappearing clauses
and publishes elimination/blocking advice, but also marks surviving replaced
clauses: retain Kissat's narrower surviving-rewrite contract. Both actual
reference calls pass three duplicate-copy fixtures (6234/6235).

Source `.claude/worktrees/runtime-substitute-retry-6231` is an immutable isolated
successor of6169. Build manifest6231 binds all files, zero-context incremental/
cumulative patches, logs and independent pins. Release/BC SHA256:
`955b2f10bd53648ef425414aec2b399332e3da0c8bafb5ddbd6c7da757407024`;
trace: `19229967088768a49bbb320535571a29cf8243a395d9709970e9cee7a35c01ef`.
Accepted production source/binary remain unchanged.

## Correctness evidence and test consideration

New `test_substitute_retry` reproduces missing advice on actual frozen parent
objects in all36 cases; negative control returns success only when the expected
missing effect is observed, not via an unexplained crash. Repaired Release
103/103 twice and trace102/102 pass. The same36-case fixture also passes with
the forced image path in each build. Cases cover no-proof/DRAT/LRAT, canonical/
public instances, both import orders, physical multiplicities1/2/5, no-op
reapplication and true/false guard states. Unchanged binaries and surviving
rewrites must not re-arm their other endpoints.

6244 adds twelve independently checked DRAT/LRAT proofs with the actual frontend,
an explicit Substitute call before an independent fixed PHP(4,3) refutation,
both rewrite paths and three input multiplicities. Each invocation also checks
ten reconstructed models and a unit-collapse case proving publication happens
after the representative becomes root-fixed. All generated successful fixture
CNFs/proofs were deleted; hashes and checker outputs remain in6244 summary/logs.

Fixture/tool failures are retained, not solver findings:

- The first C++ fixture called nonexistent `Proof::close`; use the actual flush
  API and ordinary destructor. Compilation failed before execution.
- Native fixture passed; CaDiCaL6234 aborted before decomposition because this
  pinned API requires explicit variable declaration when factorcheck is on.
  Successor6235 declares four variables and passes; reference source unchanged.
- Direct kernel construction in6241 omitted input clause IDs, producing rejected
  LRAT.6242 proves the entire malformed proof byte-identical on the frozen parent.
  Parser-based6243 passes both proof modes, then its input-copy-count assertion
  fails because the frontend normalizes duplicate binaries.6244 measures the
  physical post-import multiplicity and all twelve certificates pass. These are
  fixture setup corrections, not solver fixes or checker acceptance relaxations.
- 6247 initially assumed `production_B`; the frozen matrix names its production
  arms `current_B/current_C`. Corrected before publishing the checkpoint.

6245 removes exactly six reproduced setup CNF/proof artifacts (1502 bytes), with
hashes, source and logs retained; they are reproducible, not recoverable via trash.
No benchmark campaign artifacts or frozen evidence were manually deleted.
Changed-source Claude-owned fuzz remains open and blocks promotion, not further
isolated development. No denied transfer retry or independent fuzz run occurred.

## Causal and runtime limits

6237/job1677192 is complete/clean.6246 verifies the inherited69,828,932-row
300K hash and all253 overlapping rows, then compares the full suffix against
cached native6183. First divergence remains conflict386968, after78,180,080
matching decisions: at decision78026562/level30 Tactix chooses17958 and native
10307. This is195 decisions earlier within the phase than parent6107's split.
Restoring the producer does not establish heap, trajectory or runtime closure.
Existing earlier Sweep duplicate-count differences remain unexplained by this
repair. No timing or final-proof claim is made from stopped diagnostics.

6248 is a successor stopped-schedule probe: retain a nonstopping300K prefix,
then intentionally exit after initial BVE membership at386968. Compare complete
members, scores, occurrence pairs and the five formerly missing variables
against cached native6208. Do not mistake its intentional schedule stop for a
missing1M replay or infer tie-breaking from partial members.

At frozen6247 checkpoint,6236/job1677191 has all66 expected outcomes,64 checked;
B/C ratios0.726888/0.929090 versus production.6239/job1677258 has35/50 solver
records,30 checked; no partial full-set score. Both own fresh pins and run with
asynchronous proof checking. Full50 uses the fixed7239.010-second Kissat vector,
not a reference rerun. Three paired timing waves and complete gates remain
required for a causal performance claim. Prior6169 remains48/50 checked at
1.160932x fixed reference; it is not this repair's runtime result.

Next: validate6248 incoming membership, then localize the first remaining BVE
producer/order difference; inspect full50 work/memory and run targeted repeated
timings. Rank50 actual admission inputs at823562 remain a separate high-value
lead. Private FlatLists redesign stays unapproved; no architecture change,
production promotion, push or broad cleanup. Goal ≤1.01x is not met.
