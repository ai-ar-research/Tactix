# Composition does not close the current Rank32 forward discrepancy

Follow-up to `2026-09-08-reduce-root-runtime.md`; earlier report stays frozen.

5925/job1675145 completed normally. Its complete early/suffix native comparison
still matches68,091,135 decisions, then differs at271085/67954350 with
Tactix-29407 versus native-29066. Separate `parent-replay.json` binds the actual
candidate5900 and5920 records, verifies all four full ordinary trace hashes
and confirms identical partial proof bytes through300K.

Thus the composition has no observed effect anywhere in this bounded Rank32
run. The second-forward discrepancy at271079 also remains unchanged. This
rejects preconnection cleanup as the explanation of this local discrepancy;
do not claim a longer prefix, closed boundary or composition speedup.

Next, identify the first different clause/partner work within that second
forward call, including candidate eligibility, root satisfaction and budget
exit behavior. The heap differences are downstream evidence, not grounds for
changing tie rules. Full5926/job1675162 andBC5924/job1675144 remain active and
may show later effects; do not cancel or score them from this partial result.
5900's complete1.112038x score and all correctness/fuzz limitations remain as
reported. No source, runner, pin or old report was changed.
