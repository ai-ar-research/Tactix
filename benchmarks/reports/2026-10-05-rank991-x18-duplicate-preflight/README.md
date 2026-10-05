# Rank 991 X18 duplicate-only preflight

The generic `GraphIntervalEquivalence` X18 action does not add information to
fixed portfolio rank 991, `abw-K-dwt__234.mtx-w50`.  CPU-Q job `1750541`
selected scalar bound 1024 as the first controller action, before
`ResumeSearch vmtf/adaptive-glue/saved`, but all 232 binary directions from the
116 authenticated interval groups in that first census were already live.
The action therefore returned `NoOp`, emitted no structural-action provenance,
performed no RUP replay, and added no clause or proof literal.

The exact first request is feedback v2 sequence 1 with instance 1024 and
invocation
`sequence-repeat=once:1 step=1:GraphIntervalEquivalence:0:once:1:0:1:post(1):rulework(0,0):material(0,0):inherit:instances[1024]:grids[]:sr[]`.
It took 187,915,092 ns.  Sequence 2 resumed the sole named search
configuration.  The controller trace records the same request/result with
elapsed time 187,902,249 ns.  This establishes action selection and ordering;
it does not establish a treatment effect.

The 30-second preflight ended `UNKNOWN` after 20,245 decisions, 6,157
conflicts, 5,827,359 propagations and 145 restarts.  The controller requested
X18 97 times; 96 censuses recognized the same structure and the final census
was interrupted by the solver deadline.  Cumulatively they found 11,136
groups and duplicate-suppressed 22,272 directions, with zero selected groups,
zero RUP ticks/rejections, zero additions and zero proof literals.  Discovery
charged 551,346,282 work, 15,993,036 source-clause visits, 46,361,077
source-literal visits and 7,624,880,288 conservative cumulative workspace
bytes.  No census hit a resource cap.  X18 consumed 17.505 seconds, 58.4% of
the solve interval, so repeating this unchanged census is actively harmful on
this target.

The planned 300-second action-off/action-on pair was deliberately not launched:
the agreed preflight condition required a certified publication.  No SAT model
or UNSAT result was produced, so there is no model check and the partial DSR
stream was not sent through the checker route.  The result rules out missing
duplicate-interval binaries as the explanation for the retained graph
constructor's checked 12.136-second SAT result.  Any next organic rank-991
mechanism must expose different Boolean information or account for the
constructor's coordinated choices; repeating X18 or expanding partial-model
phase guidance is unsupported.

The immutable bindings and complete counters are in [manifest.json](manifest.json).
The local raw bundle is
`benchmarks/runs/20261005-rank991-x18-dsr-ablation/`; its trace, compressed
feedback, provenance, partial proof and profiles are bound by SHA-256 below.
