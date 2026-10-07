# WV2 status at 5,000 solver seconds

Exact-path matching of the89 WV2 regression instances against the saved d170b4a5 full Tier-D collection gives87 independently checked results and2 solver timeouts. These are historical 5,000-second results, not a rerun of the current checkpoint.

| WV rank | Tier-D rank | Instance | Saved5,000s | Later evidence |
|---:|---:|---|---|---|
|155|630|ramsey_4_4_19.normalised.cnf|UNKNOWN5000.005s|Current noL-free1e8295106 solvesUNSAT in0.167s, independently verified; WV2 task024.|
|166|899|mp1-Nb7T42.cnf|UNKNOWN5000.004s|Today's older0e6b95c91 full44 task031 also UNKNOWN5000.005s. Current noL-free checkpoint times out at600s; no current5,000-second measurement.|

Therefore one historical WV2 5,000-second miss remains open: mp1-Nb7T42. This is not a claim that the current checkpoint has been proven to solve every other WV2 instance within5,000s.

The current noL-free regression uses600 solver seconds, not5,000. Its other8 solver timeouts have historical checked solves: sted5_0x1e3-20(4759.270s),6g_6color_366_050_04(940.071s),snw_16_9_preOpt_pre(794.040s),ntil-90d-39(4989.624s),hcp_CP18_18(1003.133s),lockchart-group2-rnd0.3-L19-K38-P8D4J1_4(1761.656s),noL-11-4(3203.498s),puzzle32_sat(2380.155s). Latest-source5,000-second status for these8 is unmeasured.

Exact identities, original Tier-D and WV ranks, collection hash, and source records are in comparison.json. Current evidence is benchmarks/runs/20261007-nol-retired-1e8295106-regression-wv2-89/results/{024,035}.json and benchmarks/runs/20261007-final-0e6b95c91-fixed44/results/031.json. A proof-check timeout is distinct from a solver timeout.
