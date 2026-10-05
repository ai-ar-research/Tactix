# LymphoSAT SAT constructor and guidance cohort

Campaign `benchmarks/runs/20261005-lymphosat-sat-cohort` (SLURM job
`1750477`) compared the current default solver with narrowly enabled retained
LymphoSAT mechanisms on six SAT targets. Each arm had a 180-second solver cap;
every reported SAT model was checked independently.

The graph-labeling constructor recovered Tier-D rank 991 in 12.136 seconds
(model check 0.262 seconds), while the default arm timed out. This is terminal
constructor evidence only: the action performed no continuing CDCL search and
published no Boolean consequence.

The noL constructor/guidance profile recovered WV2 rank 223 in 18.327 seconds
(model check 0.059 seconds), versus 67.838 seconds for default. It did not close
ranks 401, 184, or 221. On rank 222 it regressed from a checked default SAT in
45.585 seconds to timeout. The noL recognizer fired on every treatment call:
264, 277, 271, 285, and 30 calls for ranks 401, 184, 221, 222, and 223. Nearly
every nonterminal call consumed its work allowance.

Partial-model phase inheritance did not causally change a recorded CDCL
decision in this cohort. LocalSearchGuidance installed targets on 176, 183,
180, 190, and 20 applications. The corresponding guided-decision counts were
3,697, 6,267, 4,913, 4,343, and 305; every decision matched the phase already
chosen by the native heuristic and zero overrode it. Native phase updates
superseded the installed target 59,258, 57,594, 56,871, 62,477, and 7,074
times. Walk imported no candidate on any arm. Rank 223's terminal `nolgraph 1
SAT` accounts for its recovery; the data do not support expanding CDCL partial
model guidance.

This campaign remains causal/reference evidence. It does not satisfy organic
integration because neither successful constructor published certified Boolean
information for continuing CDCL.
