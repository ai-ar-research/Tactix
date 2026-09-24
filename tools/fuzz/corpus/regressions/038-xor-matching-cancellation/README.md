# XOR matching after cancellation

AM341 bounded owner handoff, seed `343401436`, 27 variables / 80 clauses.
The unchanged AM339 Release and AM341 Debug pins emit rejected native DSR.
Run `tactix --random-policy=343401436 case.cnf proof.dsr` under SLURM;
both backward and forward pinned dsr-trim reject the pre-repair certificate.
Input SHA256: `020421a5a1ba5521e9e7d8aeace1b5cea841e1430db0597bead213f215c61391`.

GDB maps the rejected binary to Congruence's semantic XOR matching-contradiction
ladder. It used only the reduced key, omitting inputs cancelled in an original
gate. The shared repair quantifies cancellations from both gates before deriving
their equivalence or opposing units. `test_congruence_xor_cancellation` supplies
small direct API cases with exhaustive models and an independent every-addition
RUP oracle. The original run/replay and checker evidence remain frozen in
`benchmarks/runs/20260924-am346-dsr-population`.
