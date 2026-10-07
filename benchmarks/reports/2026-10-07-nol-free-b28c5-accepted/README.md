# noL-free B28+C5 accepted

The maintained scorer accepts all33 dispositions on immutable noL-free1e8295106, binary27cf0cab924c52479d6c11885d284ae461d6c4911ef2e557847f58bad87e9469, using only `--auto-proof`. There are17 independently checked models,15 independently verified UNSAT proofs and one clean allowed proof censor. Total solver time937.794s is below999.87615s; no missing rows, wrong verdicts, checker rejection, cleanup failure or cost-gate failure.

Campaign `benchmarks/runs/20261007-nol-retired-1e8295106-regression-b28c5`, Slurm1754058, is terminal. Task32 exits1 because its proof pipeline is censored; the standing scorer explicitly accepts that disposition, and it does not certify the instance. Exact manifest/scorer identities and proof-cost comparisons are in score.json.

This qualifies B28+C5 compatibility for noL retirement with the residual seed default unchanged and generic graph construction disabled. The diagnostic original-seed policy, later FastEliminate changes and graph Window adapter are outside this pin. Other regression suites, full Debug and current Claude fuzz remain pending.
