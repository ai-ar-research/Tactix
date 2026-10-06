# Native repeated DirectCSP WV93 causal probe

SLURM full-width array `1751453` compared one versus four deterministic startup
support actions on the existing WV100 rank93 RB-SAT representative. Both arms
used the immutable Release source `bf21ed1e98478b00c92fe314754e60362815cfe8`,
solver SHA-256 `5645864b8bd653157fc7e4e0939b9789e94933329a44dfaa8bee50bf91ff3ec2`.
DirectCSP rule, native producer and reference-profile files have no diff against
the current accepted implementation. No production source or default changed;
no build was created. Parent-provided post-cleanup usage was 440,253,227,520
bytes, below the 700,000,000,000-byte launch guard.

The hypothesis was that a small deterministic repeated batch would yield more
useful Boolean information than one row. Both native producers returned eight
exact offers after 29 row candidates and 4,476,027 metered work. The standard
10,000,000-work page therefore admits this representative. The four-row prefix
reused its first census through propagation and advice, and selected four
unique exact payloads.

| Startup measurement | One row | Four rows |
| --- | ---: | ---: |
| Support additions / distinct payloads | 1 / 1 | 4 / 4 |
| Propagate actions | 1 | 4 |
| Propagation ticks / new root assignments | 0 / 0 | 0 / 0 |
| Advice applications | 1 | 4 |
| Advice variable occurrences | 19 | 66 |
| Support proof work / literal occurrences | 24 / 19 | 96 / 66 |
| New extension variables | 0 | 0 |

Both arms resumed CDCL and returned UNKNOWN at the authorized 30-second limit
(exit 124; wall times 30.02 and 30.05 seconds). Whole-run counters were
264,912 versus 274,098 decisions, 178,304 versus 177,439 conflicts, and
16,536,568 versus 16,732,275 search propagations. Native learner scheduling
continued after the prefix and eventually published seven versus eight unique
support clauses, each followed by advice and a nonproductive Propagate action.
These later actions and the single timing window prevent isolating a search
benefit from the initial batch. This establishes native lifecycle reach,
unique publication and advice consumption, but no root deduction, solve gain,
original-formula SAT model or UNSAT closure.

The runner independently hashes the original decompressed CNF and invokes the
pinned original-CNF model checker on any SAT exit. Neither arm returned SAT;
model checking is therefore not applicable, rather than passed. DSR streams
were emitted and retained (7,489,772 / 6,058,042 bytes); this probe did not
independently validate their lemmas or any UNSAT certificate. Existing accepted
focused checks remain the proof authority for the implementation.

Array `1751451` failed before executing a solver because the earlier frozen
causal README transcribed the decompressed hash incorrectly. The retained old
`results/input-cnf.sha256` and independent decompression agree on
`00f51d5059170d838edca17715419c9ff937ae5f22c4c575508d3fd99b4bc7ca`.
The compressed hash agrees with the old report. The new manifest uses the
verified original-input hash; frozen older artifacts were preserved.

[Results](results.json) own exact counters. The uniquely named
[probe directory](../../probes/20261006-directcsp-repeated-native-v1/) retains
profiles, executable runner, exact argument vectors, immutable-pin identities,
raw controller traces, compressed raw feedback, provenance, proof streams,
stdout, resource use and failed-preflight logs. Feedback extraction covers the
startup prefix through first ResumeSearch; whole-run counters come from solver
stdout and full provenance. Raw feedback was retained without a full replay
check. [SHA256SUMS](SHA256SUMS) binds retained probe artifacts.

The next useful step remains after certified publication: obtain a bounded
support consequence or relation batch that produces actual Boolean deduction,
or isolate an independently checked search benefit. Do not extend the target
runtime or promote DefaultPolicy from this negative short probe. Effective
Claude-owned both-policy fuzz and cumulative integration gates remain open.
