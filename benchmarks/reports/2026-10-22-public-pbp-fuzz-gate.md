# PBP public-strategy fuzz gate — report to Andrew

Candidate `5b9075e1d4af74d58d4710e186d4ed662edca2fd` on
`codex/cg-closures` was qualified with matched Default and public-random fuzz
campaigns. The candidate is pushed to `origin/codex/cg-closures`; this report
does not claim that it has been merged to `main`.

## What changed

The production CLI now contains only `DefaultPolicy`. The fuzzing strategy is a
separate client built solely on the public `tactix.h` interface. It generates
rich executions from the root/search/restart grammar, invokes one public action
at a time, and re-queries the solver after every action. The client mixes
canonical, instance, semantic and one-step online calls and exercises root
simplification, structural and algebraic reasoning, local-search guidance,
CDCL search, and partial/full restart transitions. Retired production random,
stress and controller frontends and their one-off scripts were removed; only
test support still needed by focused structured-action unit tests remains.

PBP is the fuzzing default. Every UNSAT result is checked independently with
VeriPB and CakePB, and every SAT model is checked against the input CNF. The two
campaign arms use the same deterministic generated instances. Stable failures
remain in the regression corpus, including the new duplicate-binary PBP case.

The cluster runner submits an unthrottled 64-shard array to `cpu-q`, with 13
workers and 8 GiB per worker in each shard. Slurm, rather than a hard-coded core
ceiling, controls simultaneous placement. The all-user storage governor stayed
active and in released mode; it controls pending jobs only and never pauses a
running job.

## Candidate and oracle pins

- Default binary SHA-256:
  `4cacef9795cb85893fc0def8385bf21dfb5afee9addd26b8d4eb7fa765a5acad`
- Public-random binary SHA-256:
  `b10055beda184455198eeae0fbf5044ef5451e21f947ee0bbf5695865e9f3a08`
- Shared harness SHA-256:
  `73d5951a21faa65d2b1cec53fa8bf2212b90b1194106fcda68d08191f15acf48`
- Frozen clausal checker set:
  `895f1989468d5e01a81fa647f9cb9739448b81507a4b78515e4e46a7441c44f8`

The local focused Debug and Release tests passed, as did the Slurm sharding and
aggregate tests. A matched local 100-case screen produced the same 70 SAT and
30 UNSAT answers in both arms with zero failures. The full 52-entry regression
corpus produced zero crashes or correctness failures under the public-random
client; one crash-only entry remained a nonverdict at its 30-second limit.

## Cluster results

| Arm | Array / finalizer | Generated | Independently checked | Nonterminating | Failures | Result |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| Default | `1795957` / `1795958` | 200,000 | 199,849 | 151 (0.0755%) | 0 | documented gate met; see caveat |
| Public random | `1795966` / `1795967` | 200,000 | 196,597 | 3,403 (1.7015%) | 0 | aggregate PASS |

The public-random aggregate passed all 64 shards. Its coverage included 145,134
cases with root actions, 146,426 with search actions, 151,263 root-to-search
transitions, 87,391 partial restarts, 112,013 full restart/search-to-root
transitions, and 146,037 cases with algebraic actions. Canonical, instance,
semantic and online forms were each reached by more than 153,000 cases and were
effective in more than 143,000 cases. All required action and transition gates
passed.

The Default finalizer was accidentally submitted with a stricter requirement
of exactly 200,000 verdicts and zero nonterminating cases, so it truthfully
printed `FAIL` after the 151 ten-second timeouts. That configuration is stricter
than the documented release gate of at least 100,000 checked seeds per arm.
The run nevertheless supplies 199,849 independently checked results and zero
correctness failures. One retained timeout (shard 3, index 11793, seed
20272502) was replayed unchanged against the pinned candidate with a 120-second
limit; it completed UNSAT in about 75 seconds and passed the proof oracle. The
red finalizer record is retained rather than rewritten.

## Storage and disposition

At the terminal check, quota use was 373 GiB of 1 TiB. The Default artifacts
used 5.3 MiB, public-random artifacts 35 MiB, and the two pinned binaries 11
MiB. The storage governor held no jobs and reported no control errors.

The new public-interface random strategy is therefore part of the normal PBP
fuzzing pipeline and its full aggregate gate passed. The candidate should not
be merged to `main` without explicitly accepting the Default finalizer caveat;
no additional broad rerun is needed to interpret the completed evidence.
