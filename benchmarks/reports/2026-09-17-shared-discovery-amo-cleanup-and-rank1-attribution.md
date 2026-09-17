# Shared discovery, AMO cleanup, and Rank 1 Satsuma attribution

Checkpoint 17 September 2026 UTC. Phase 1 remains open. This report freezes the
AM55--AM57 decisions and the first exact Rank 1 comparison with the owning
Satsuma+Kissat baseline. No full-suite acceptance or production promotion is
claimed from focused cases.

## Coherent source checkpoint

AM55 made SR discovery effort one explicit non-refilling allowance shared across
the native structured-root sequence. Already admitted work can finish, but later
producers cannot silently refill discovery credit. Authority:
`build/20260917-112631-am55-shared-sr-discovery-authority/manifest.json`, source
tree SHA256 `1758c3ef2689f1343129ff9131f1a8b7a97ad472c70174ea16157cac64857ed8`,
bit-identical main SHA256
`c1ebd2a612713453083e2d51bd2e08c6fa7ed1ff4e29533e8d90973522d178a1`.
Comparable hwu Rank 6 repetitions are 113.937/112.246/113.887s, all below the
frozen Satsuma+Kissat 203.321s case gate. A zero-SR diagnostic takes 68.457s,
showing that roughly 45s of AM55 preprocessing remains avoidable when no SR
consequence is published. AM55 also checks Rank 29 in 363.327s; Rank 26 still
times out at 1,200.005s after 1,093 useful AMO actions.

AM56 adds a monotonic kernel statistic for binaries published by the native
unique-literal AMO certificate. Caller-supplied SR cannot increment it. After the
structured consumers, DefaultPolicy runs one existing canonical BCE pass only
when that statistic changed. This provenance trigger is general and causal:
Rank 26 publishes 1,089 AMO binaries, BCE deletes 1,084 blocked helpers in
73,022 ticks, and the original-CNF model verifies after a 399.586s solve. B28
rank 7 publishes no such helpers, skips BCE, and verifies in 3.672s. Authority:
`build/20260917-120206-am56-native-amo-bce-authority/manifest.json`, 343 source
files, source tree SHA256
`02093cb73198632e00c4fcbae5c14d34e8f894a46f0507172bb92d6a9bfe11bf`,
bit-identical binary SHA256
`2a2fbb4c355a5c741bcdd92c09c09b7d45c3579b0b9b3448f1c086b28cda09d0`.
Release 161/161 passes in 199.69s; focused ASan/UBSan/leak and fixed native
proof/model checks pass. The root was restored to all 343 manifest hashes before
this checkpoint.

AM56 focused run `20260917-120419-am56-native-amo-bce-screen` is complete:

| case | seconds | independent result | structured observation |
|---|---:|---|---|
| Rank 26 | 399.586 | SAT model verified | 1,089 AMO binaries; one BCE cleanup |
| Rank 1 | 6.025 | UNSAT proof verified | 615 SR clauses; no AMO/BCE |
| Rank 6 | 122.575 | SAT model verified | no SR output; shared 20B discovery allowance exhausted |
| B28 rank 7 | 3.672 | SAT model verified | no unrelated cleanup |
| WV88 | 92.482 | UNSAT proof verified | 538 effective SR actions |

These focused cases close the known Rank 26 timeout at this candidate level and
preserve the Rank 6 case gate. They do not replace the complete Rank and B28+C5
vectors.

## Rejected fixed generator prefix

AM57 changed DejaVu to return after a fixed prefix of 128 generators and relied
on the existing scoped fixpoint to repeat only after publication. Run
`20260917-121609-am57-progressive-generator-prefix` independently verifies all
four outcomes:

| case | AM56 seconds | fixed-prefix seconds | decision |
|---|---:|---:|---|
| Rank 1 | 6.025 | 9.580 | reject regression |
| Rank 6 | 122.575 | 324.775 | reject regression and case-gate failure |
| Rank 26 | 399.586 | 306.742 | local gain |
| WV88 | 92.482 | 41.321 | local gain |

The fixed prefix changes which subgroup consequences reach the formula. WV88
benefits from productive repeated prefixes, while Rank 1 repeats six passes and
Rank 6 publishes a large changed formula that worsens search. A universal fixed
chunk is therefore not a safe effectiveness measure. Its frozen authority and
run remain decision-changing negative evidence; the source checkpoint excludes
AM57 and the later unsubmitted validation-cap prototype.

## Why Satsuma+Kissat solves Rank 1 in 1.218 seconds

The frozen baseline record is
`benchmarks/baselines/sc24-26-32g-5000s/results/00563.json`: 1.218s total wall,
267,452 KiB peak RSS, verified UNSAT proof, on the same 85,230-variable,
276,273-clause input. Plain frozen Kissat takes 0.216s, so Satsuma spends roughly
one second to change a formula that Kissat already solves quickly.

Diagnostic job 1699170 reruns the exact frozen Satsuma and Kissat binaries on the
exact cached input under cpu-q/c8/32 GiB solely for phase attribution. It records
1.09s Satsuma and 0.18s Kissat. SLURM labels the job FAILED because the shell's
`set -e` propagated Kissat's standard UNSAT exit code 20; both tools completed,
the expected UNSAT line and timing records were captured, and the trap removed
the input, output, and proof scratch. The historical independently checked
baseline remains the correctness authority.

Satsuma's verbose phase record is decisive:

1. Unit preprocessing removes one input unit; ULC adds no AMO clause.
2. Its direct-binary graph has 423,975 vertices and 1,566,604 directed edges.
3. First detection takes the cumulative clock from 0.26s to 0.53s and returns
   614 generators.
4. At 85,230 variables, the source selects its `binary_fixing` path because the
   baseline default `schreier_domain_limit` is 50,000. Negation fixing emits one
   unit; binary fixing emits nothing.
5. That unit earns one repeat. The second graph has 613 generators; both fixing
   passes emit nothing, so preprocessing stops. Total Satsuma profiling is
   537.81ms DejaVu, 156.67ms refinement, 138.95ms deduplication, 84.94ms
   Schreier, 80.99ms structured detection, and 1,071.03ms overall.
6. The output has 85,230 variables and 276,258 clauses. Kissat proves it UNSAT
   in 0.18s in the diagnostic window.

AM56 behaves differently. Its 6.025s Rank 1 solve spends 5.775s in the structured
root and 5.614s in unattributed structured work. It applies 615 SR clauses and
then needs one conflict. Application costs only 36ms; the overhead is discovery
and planning. Inclusive traces report 2.936s for orbitopal generator-orbit work,
0.956s generic detection, 1.486s structured discovery, 1.103s row generator
fallback, and additional snapshot/validation work; nested values must not be
summed. Satsuma avoids the full clausal chain and publishes only the useful unit.

The implementation target is therefore the owning baseline's general
large-domain policy: cheap negation and binary fixing, followed by another pass
only after a certified published consequence. The public unrestricted graph-SR
operation remains available under N1. The policy should also retain Tactix's
prepared useful-inference admission, which previously solved and verified Rank 1
in 0.317s before SR. Matching Satsuma does not require discarding a stronger,
cheaper native closure.

## Remaining gates and next experiment

The last complete coherent performance vectors remain AM36: B28+C5 33/33 in
1,414.317s and Rank 49/50 in 10,520.336s, both above their 952.263s and
8,115.735s targets. Later focused Rank 29 and Rank 26 recoveries are not a full
vector. The next candidate must implement the large-domain fixing branch through
typed strategy controls, with the 50,000 threshold owned by Satsuma source rather
than inferred from this benchmark. First measure Rank 1, Rank 6, Rank 26, B7 and
WV75/77/88 with independent checks. Only a coherent focused result earns the two
complete Phase 1 arrays.
