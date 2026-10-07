# B28+C5 final recovery checkpoint regression

Runtime source76e1f68b8, job1751524, frozen campaign20261007-final-76e1f68b8-regression-b28c5. Owning scorer accepts33/33 rows with zero failures or missing results:17 independently checked SAT models,15 independently checked UNSAT proofs and one bounded proof censor. The censor is not a verified UNSAT certificate. Aggregate solve time972.471seconds is below the saved999.87615second ceiling. All input/binary/checker/baseline bindings and per-row cleanup/resource/verdict contracts pass scorer verification. Exact score, manifest, submission and33 result rows are copied here.

Scheduler task32 is FAILED/exit1 because its T105.2.0 proof check reaches the standing1,800-second deadline; all other32 tasks complete/exit0. The task wrapper reports unverified checking as nonzero, while the owning regression scorer explicitly accepts this bounded censor. No checker rejection or cleanup/infrastructure error is recorded.

This accepts B28+C5 only on the pinned recovery source. Rank1–50/WV100/WV2 and effective Claude fuzz remain pending; full Debug retains documentation-size debt. It does not accept the later opt-in split-budget Walk working tree or close another fixed44 target.
