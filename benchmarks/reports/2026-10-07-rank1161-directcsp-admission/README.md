# Rank 1161 DirectCSP bounded admission: not admitted

SLURM job `1751725` completed the reviewed standalone census with exit 0, using
10 seconds, 10M enumeration work, 100K original source clauses, 64 domain candidates
and 16 offers. It executed no SAT solver. The full compressed input hash matched
the frozen four-winner authority:
`0f 1c 5f 7b 89b 52a 0b 2b 1d 2421793e 5502302b 2e 05c 95c 7 bb 5 ee 381e 8a 02264915`.
The original input contains 2, 248, 832 variables and 4, 534, 906 clauses.

The census spent 4, 552, 599 work and 6.312 internal wall seconds (6.44 process
seconds, 116, 724 KiB peak RSS). It authenticated 64 exact-one domains and 256 exact
OR definitions, but produced **zero lifted nogoods and zero nontrivial path
nogoods**. No proof candidate or model was independently checked. The target
remains open, and native implementation is **not admitted**.

## The causal truncation boundary

All 256 admitted OR definitions are complements of key values within the first 64
one-hot domains: for example `-1 <-> (2 OR 3 OR 4)`. These definitions are valid,
but do not recover the auxiliary lock predicates intended by this investigation.
No admitted OR output is a positive auxiliary variable.

A read-only review of exactly the same 100K-clause prefix found 7, 040 positive
auxiliary OR reverse clauses, with at most 5 inputs each. Thus the immediate
failure is not absence of those OR premises in the prefix, nor the 64-value width
limit. The first real lock predicate is
`10145 <-> (1 OR 33 OR 129 OR 737 OR 3105)`.
Its forward implications are original occurrences 17753--17757, and its reverse
clause is occurrence 17758. Its 5 exact key domains have zero-based ordinals
0, 8, 32, 184, 776. The latter 2 fall outside the first 64-domain page. Their ALO and
all 6 AMO occurrence identities are authenticated in
[prefix-review.json](prefix-review.json). The original tool rejected genuine
lock OR definitions whose inputs were outside its selected domain page.

There is a second, independent missing-premise boundary: the same prefix contains
**zero binary clauses beginning with a negative auxiliary literal beyond the
key extent 10144**. In particular it contains no outbound binary premise from
auxiliary 10145, 10146 or 10147. The proposed lift needs such a connecting premise
before it can connect one key value to another domain's excluded value. Merely
replacing the first 64 domains by these 5 related domains would recover real OR
meaning but still would not admit this deduction mechanism from the prefix.

The retained mechanical-master-key reference detector reads the initial key
ALO/AMO region, then all OR definitions, then looks for these signed auxiliary
implications in the later formula (`solver.cpp`, detect, lines 70--150). This
supplies a precise source-derived query to investigate, not proof that the
unseen suffix contains a useful connecting clause. No suffix was inspected.
The 2, 536 contiguous positive key-domain count in the review is a candidate shape
count; only the selected example domains and original 64-domain page have been
independently authenticated here.

## Conditional next admission

One bounded **indexed** census would be justified only if an already complete,
original-input-hash-bound clause-occurrence/implication index is available. Seed
it with actual auxiliary 10145 and the 5 authenticated input domains; retrieve
its signed outgoing original binary premises and exact neighbor owners. Within
the same caller envelope, look for an absent final binary nogood backed by an
ALO and explicit incompatibility coverage, retaining at least 2 supports for each
endpoint and excluding original binary-reachability consequences. Any lifted
helper must retain its own exact original implication premises. Independently
check the resulting RUP prefix before implementing a native action.

No suitable index was identified by filename searches under `benchmarks/cache`,
`tools/bench` and `tools/solvers`. Therefore no follow-up launch is prepared or
submitted. Do not substitute a full scan, a larger prefix/domain budget or a
long solver run for the missing index and connecting-premise evidence.
A negative prefix result cannot classify the unseen suffix or the whole target.
Recognition and syntactically new binary shortcuts alone do not pass the gate.

[gate.json](gate.json) owns the disposition. [artifacts](artifacts/) preserves
byte-identical census, command, outcome, resources and logs; the original launch,
pins and result remain untouched. [launch-manifest.json](launch-manifest.json)
binds tool snapshots, source-review commit and input authorities.
[review_prefix.py](review_prefix.py) reproduces the additional bounded review.
[SHA256SUMS](SHA256SUMS) binds this report bundle and the retained reference source.
