# Progress

The plan owns scope/gates, calculus owns semantics, implementation notes own
guards/representation, the parity ledger owns N4, and frozen reports own evidence.

## Current resume anchor

- Goal: **Rank 1–50 aggregate runtime ≤1.01x Kissat**, with B28+C5 regression
  gates and general repairs. **Not met. User resumed after Sep10 06:00EDT; new cutoff pending.**
- [Runtime review](benchmarks/reports/2026-09-07-runtime-review.md) owns the
  audit of Claude's work and retained/rejected evidence. Mixed proof modes and
  unchecked serialized arms do not establish “93% search quality.”
- New isolated [assignment-level5672](benchmarks/reports/2026-09-08-assignment-level-bound.md)
  stops the exact antecedent maximum at the current-level bound;root maximum is zero.
  Release90/90 twice;trace89/89;3288 old/new states agree;5677 full1M parent replayPASS.5679 three-pair median1.030478x,6.01% candidate spread: no proven speedup.
  [Current5741](benchmarks/reports/2026-09-08-forward-unit-exit.md):92/92 Release twice,91/91 trace,5088 checked old/new proofs;5755/5756 full parent trace/proof replayPASS. Full5758 all40 solves7807.759s=1.146503x fixedKissat,29 checked at snapshot;not promoted.
  Fixed Kissat authority: `benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json`:7239.010s,original40's6810.066s unchanged plus matched6108 additions. Future full50 views Tactix-only at12CPU/48000MiB;pending added certificates block acceptance,notdevelopment. Historical5712 Tactix1.110877x over40,80/80checked;5758 now40/40checked.
  [Current5962 runtime](benchmarks/reports/2026-09-08-bve-runtime-and-repeated-pop.md):full5970/job1675801 all40 checked,7603.117s/6810.066s=1.116453x fixedKissat. [6037 seal](benchmarks/reports/2026-09-08-retained5962-checked-gates.json):BC66/full40/reference40 checked/clean;original timing snapshots unchanged. All40 certificates/other non-timing stats exact5900;only BVE/deactivation counters differ on8 ranks. BC5968 B0.743989x/C0.931615x production.54 fixed proofs/four suitesPASS. General inactive-pop repair retained provisionally,not a runtime gain orpromotion;reject5920 composition1.176794x/C1.017340x.
  [Parent5522](benchmarks/reports/2026-09-08-sweep-small-environments.md) closes
  all18,593,182 Rank32 decisions through19376. Full5535 is1.162180x Kissat,
  all80 checked/clean;5599 three pairs median0.824875x5522,spreads1.60%/0.52%,all6 proofs checked.
  Fuzz5765 on5741 has no terminal verdict:8493/38524 ended interim;96672 failed ENOTFOUND.5777 network retry denied;explicit external-transfer approval required,no bypass/artifact inspection.5715/4746 terminalPASS5672. User-approved paired fullTierD5784/job1673402 runs2332 tasks oncpu-q,4CPU/32GiB,1200s solve/5h allocation;independent frozen pins.
- Preceding implementation: [Sweep membership cursors](benchmarks/reports/2026-09-08-sweep-membership-cursor.md), candidate 5473,
  removes repeated retained-prefix scans from Sweep's private ledger on 5466.
  Both Release suites pass 84/84; 588 actual old/new ledger fixtures agree.
  [Measured timing and causal follow-up](benchmarks/reports/2026-09-08-sweep-measured-and-causal-followup.md):
  three exact-work pairs give median 0.984563x predecessor; all six proofs checked.
  [Full comparison and causal witnesses](benchmarks/reports/2026-09-08-full-sweep-and-causal-witnesses.md):
  5485 has all 40 pairs at **1.165337x Kissat**, 59/80 checked at snapshot.
  All 40 candidate non-timing statistics/certificates match bound5459 exactly.
  B28+C5 is 66/66 checked/clean, combined 0.953757x production Tactix.
  Current-source fuzz remains pending; no source is promoted.
- Preceding [Watch-pair append](benchmarks/reports/2026-09-07-watch-pair-append.md), candidate 5466,
  adds an exact-layout two-word fast path within existing capacity on 5459.
  Both Release suites pass 83/83; 180 actual old/new cases have identical
  whole-state digests. [Measured timing](benchmarks/reports/2026-09-08-watch-pair-measured.md)
  now gives three exact-work/proof-byte pairs: median 0.983980x predecessor,
  range 0.981238–0.993028. [All six timing proofs are checked](benchmarks/reports/2026-09-08-watch-pair-checked.md);
  B28+C5 is 66/66 checked/clean (0.958073x production); latest-source fuzz pending.
- Preceding [Bound lifecycle](benchmarks/reports/2026-09-07-bound-lifecycle.md), candidate 5459,
  restores the documented phase-bound advance and retry flags after quota cuts.
  Release 82/82 twice, trace 81/81; two old/new cut-bound fixtures pass.
  It retains [5450's strict phase stop](benchmarks/reports/2026-09-07-eliminate-phase-budget.md).
  Rank-37 Factor schedules match; differing gain-2 admission selects this repair.
  Causal replay now matches all 35,346,388 decisions through 100000 conflicts;
  272 portable boundaries also agree. Full runtime is now scored below;
  B28+C5 is now 66/66 checked; full-view certificate checking remains pending.
- [Complete bound view](benchmarks/reports/2026-09-08-bound-full-and-next-losses.md):
  all 40 pairs are **1.173817x**, 58/80 checked at the solver snapshot; no
  timeout/verdict loss. No aggregate breakthrough. Rank 37 is again the largest
  loss (712.017/346.009 s, conflicts 2.114745x), then Ranks 32 and 31.
  Rank 37 now first splits at 188502; earlier Sweep/Vivify boundaries are
  localized. Rank32 selected Sweep replay matches its full parent per arm;
  first14 seeds agree, then Tactix alone processes836. Eligibility5505 is validated.
  Rank 31 matches 51,491,857 decisions and 276 boundaries through 100K.
- [Short phase witness and closed gates](benchmarks/reports/2026-09-08-square-phase-and-checked-gates.md):
  Square first differs at conflict 501, decision 5849, same variable/opposite
  polarity. Replay 5482 now shows differing saved phases, not phase priority;
  Implications5487 lead to differing startup auxiliary definitions in5498.
  Factor5502 matches first942 extensions, then selects different seeds at943.
  Incoming physical formula/heap cause remains unresolved; do not alter phase priority.
- [Complete phase-budget view](benchmarks/reports/2026-09-07-phase-budget-full-view.md):
  5454 has all 40 pairs at **1.170283x**, 63/80 checked at the solver snapshot.
  No timeout/verdict loss; goal still unmet. Largest losses are Ranks 31/37/32.
  Phase-entry 5434 is now fully checked at 1.205580x; different windows prevent
  attributing their ratio difference to one repair.
- [Short witnesses and native bound](benchmarks/reports/2026-09-08-short-witnesses-and-bound.md):
  5458 confirms native bound 1→2 after the quota cut. All nine short-B solves
  are checked; old/new work, proof bytes and slot logs agree, so they are
  controls rather than cut reproducers. Square now has a short causal probe.
- [Full view and next boundaries](benchmarks/reports/2026-09-07-runtime-next-boundaries.md):
  Rephase's full 40-pair view is **1.202034x**, now 80/80 checked and clean.
  Its nine timing proofs and B28+C5 are now fully checked. Metadata 5426 has a
  small 0.993882x median exact-work gain, not a solution to the aggregate gap.
- [Root-entry follow-up](benchmarks/reports/2026-09-07-root-entry-followup.md):
  5430 restores missing root closure before Probe/Eliminate, with distinct
  search-tick/phase-saving scopes. Actual old/new scheduler fixtures confirm it.
  Independent 5426 removes redundant watch-metadata writes without state changes.
  5430 is covered by 5438's terminal fuzz pass; independent 5426 remains
  unfuzzed. Neither source is promoted.
- [Rephase closure](benchmarks/reports/2026-09-07-rephase-root-closure.md) owns
  exact Rank-35 prefix/heap closure and now-checked paired 0.658243x preceding
  candidate, 1.183228x Kissat. New clean profiles prioritize inprocessing cost.
- [Dense-entry evidence](benchmarks/reports/2026-09-07-dense-entry-cost.md) owns
  the preceding candidate: Rank-21 peak RSS falls about 16.5%, runtime is mixed.
- Correction: stopped-prefix 5382 contains dormant hot-loop instrumentation;
  its 1.13834x time is not clean production overhead. Use clean profiles 5385.
- Production remains `c2a95a6`; Release SHA-256
  `de3d9332f70f3bbd5ca17fedda4ad11c7295114e8b581251192746ff8d68439f`.
  No source was promoted. Retain supported general repairs; do not chase
  trajectory identity when work is already stronger or neutral.

## Active jobs and gates

| Campaign | Job | Pending scope |
|---|---|---|
| 5423 Rephase full Rank 1–40 | `1669887` | Complete: 80/80 checked, 1.202034x. |
| 5429 metadata B28+C5 | `1669969` | Complete: 66/66 checked, 0.924072x production. |
| 5433 phase-entry prefix | `1669971` | Complete: unchanged from Rephase through 100,000. |
| 5434 phase-entry Rank 1–40 | `1669972` | Complete: 80/80 checked, 1.205580x. |
| 5435 phase-entry B28+C5 | `1669973` | Complete: 66/66 checked and clean; 0.920290x production. |
| 5436 Rank-37 prefix | `1669994` | Complete: first decision split at conflict 29,767. |
| 5437 Rank-35 Sweep binaries | `1670050` | Complete: two duplicate multiplicities only; unique sets equal. |
| 5440 Vivify maximum timing | `1670118` | Complete: six checked proofs, median 0.995069x. |
| 5441 Vivify maximum B28+C5 | `1670119` | Complete: 66/66 checked, 0.922120x production. |
| 5447 replacement cursor timing | `1670198` | Complete: six checked proofs; median 0.976662x. |
| 5448 replacement cursor B28+C5 | `1670199` | Complete: 66/66 checked, 0.919289x production. |
| 5449 Rank-37 elimination limit | `1670217` | Complete: meter exceeds phase limit by 26 before extra round. |
| 5453 Rank-37 budget prefix | `1670291` | Complete: repaired boundary, next split at Factor, conflict 32,073. |
| 5454 phase-budget Rank 1–40 | `1670292` | Complete:80/80 checked/clean at5590 audit;1.170283x. |
| 5455 phase-budget B28+C5 | `1670293` | Complete: 66/66 checked, combined 0.935341x production. |
| 5457 Factor selected window | `1670359` | Complete: observer-neutral, first-call grids equal by ordinal. |
| 5458 native bound verbosity | `1670509` | Complete: quota cut followed by 1→2; prefix unchanged. |
| 5462 short B bound witnesses | `1670635` | Complete: nine checked/clean; old/new behavior identical. |
| 5463 Rank-37 bound prefix | `1670637` | Complete: all 35,346,388 decisions and 272 boundaries match. |
| 5464 bound B28+C5 | `1670636` | Complete: 66/66 checked, combined 0.986955x production. |
| 5465 bound Rank 1–40 | `1670638` | Complete: 80/80 checked/clean, 1.173817x; summary sealed. |
| 5468 watch-pair timing | `1670699` | Complete: six checked proofs, exact-work median 0.983980x. |
| 5469 watch-pair B28+C5 | `1670700` | Complete: 66/66 checked/clean, combined 0.958073x production. |
| 5470 Rank-31 bound prefix | `1670801` | Complete: 51,491,857 matching decisions and 276 boundaries. |
| 5471 Square bound prefix | `1670919` | Complete: first polarity split at conflict 501, decision 5849. |
| 5475 Sweep cursor timing | `1671162` | Complete: six checked proofs, exact-work median 0.984563x. |
| 5476 Sweep cursor B28+C5 | `1671163` | Complete: 66/66 checked/clean, combined 0.953757x production. |
| 5478 Rank-37 extended prefix | `1671258` | Complete: 25,001,084 common suffix rows, first split 188502. |
| 5479 Rank-32 bound prefix | `1671263` | Complete: 18,583,703 common rows, split after Sweep at 19375. |
| 5482 Square selected phase | `1671600` | Complete: negative Tactix phase save at 492; replay normalized. |
| 5485 Sweep cursor Rank 1–40 | `1671708` | Complete:80/80 checked/clean,1.165337x; summary sealed. |
| 5487 Square implications | `1671745` | Complete: differing binary reasons; parent replay validated. |
| 5490 Rank-32 Sweep | `1671777` | Complete:14 common seeds, then extra836; successor5503 validates. |
| 5505 Rank-32 eligibility | `1672156` | Complete: identical one-sided836 entry snapshots, full parent replay. |
| 5515 retained-entry trace | `1672164` | Complete:11023 common seeds,33 omitted single-clause solves. |
| 5516 retained-entry B28+C5 | `1672165` | Complete:66/66 checked/clean,0.932605x production; no solve loss. |
| 5517 retained-entry timing | `1672166/1672240/1672244` | Complete:6/6 checked; all pairs regress,median1.067134x. |
| 5529 small-environment trace | `1672251` | Complete: all18,593,182 decisions match; only2 first-call ticks differ. |
| 5530 small-environment B28+C5 | `1672252` | Complete:66/66 checked/clean,0.930592x production; no solve loss. |
| 5531 small-environment timing | `1672253/1672326/1672385` | Complete:6/6 checked/clean;three pairs improve,median0.889407x5510. |
| 5535 small-environment Rank1–40 | `1672328` | Complete:80/80 checked/clean,1.162180x;summary sealed. |
| 5537 Rank32 extended small-prefix | `1672356` | Complete via5560:18,696,140 common rows; next split20095. |
| 5544 current5522 Rank39 profiles | `1672400` | Complete:2/2 checked; exact5535 work/proof, zero lost samples. |
| 5550 Vivify census B28+C5 | `1672418` |Complete:66/66 checked/clean,combined0.927432x production;summary sealed. |
| 5551 Vivify census timing | `1672509/1672515/1672519` |Complete:6/6 checked/clean,exact work/proof;spreads exceed2%,no reliable gain. |
| 5558/5567 Rank33 prefixes | `1672426/1672434` |100K all31,494,584 decisions equal;1M first split182093, validator5568. |
| 5564 Rank32 Factor grids | `1672430` | Complete via5574: first4824 grids agree;4825 differs. Extra journal rows accounted explicitly. |
| 5565/5569 Rank32 Factor inputs | `1672432/1672436` | Both binary multisets match; second large input has only2 pose differences, no metadata difference. |
| 5597/5598/5599/5604 Factor support | `1672528/1672529/1672681/1672600` |5597 closes100K;BC66/66 checked;timing0.824875x5522;full1.137589xKissat,checks run. |

Claude's terminal report for periodic fuzz 5442 is **PASS** on source 5438:
80,000 cases, 71,907 checked verdicts, zero failures. Session 30094 is closed;
the report is authoritative, and no fuzz artifacts were independently inspected.
Claude's terminal5492 report is **PASS** on5473:60,000 cases,55,036 checked
verdicts, zero failures and360 direct reachability cases; session34566 closed.
Claude5539 returned **PASS** on5522:70,000 cases,64,364 checked verdicts,zero failures.
Sessions70659/1814 closedPASS;5610 on5592:50K cases,0failures.5657/95829 closedPASS5649;5715/4746 closedPASS5672.5765 owner CLI stopped;5777 network resume requires explicit approval;no fuzz artifacts inspected.

Keep these checks running while proceeding on a provisional-pass assumption;
revisit them at checkpoints, never label pending certificates verified.
5383/5389/5393/5395/5421/5422 are terminal and checked; diagnostics through 5424 are terminal
(5401/5411 cancelled, 5413 timed out; scratch cleaned). User interactive allocation
`1668242` is untouched. Exact audit and stop limitations are in the latest report.
5425's two profiles and 5428's six timing proofs are now fully checked and clean.

| Accepted authority | Result |
|---|---|
| [5295 starting audit](benchmarks/runs/20260907-5295-checked-runtime-audit/summary.json) | 80/80 checked/clean; starting Tactix/Kissat 1.2192x. |
| [5314 production Rank 1–40](benchmarks/runs/20260907-5314-pose-r2-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves (22 SAT/18 UNSAT); 8643.178/6759.074 s = 1.2787518x. |
| [5315 production B28+C5](benchmarks/runs/20260907-5315-pose-r2-BC-gate/summary.json) | 66/66 checked/clean; final-r2/starting time 1.01897x. |
| [5379 Vivify witnesses](benchmarks/runs/20260907-5379-vivify-retry-runtime/summary.json) | 12 checked models; 0.722370x preceding candidate and 1.186297x Kissat, one wave. |
| [5380 Vivify B28+C5](benchmarks/runs/20260907-5380-vivify-retry-BC-gate/summary.json) | 66/66 checked/clean, no timeout/solve loss; candidate/production 0.953796x. |
| [5383 Vivify Rank 1–40](benchmarks/runs/20260907-5383-vivify-retry-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves; 8217.738/6764.776 s = 1.214783x. |
| [5388 publication witnesses](benchmarks/runs/20260907-5388-dense-publication-runtime/summary.json) | 12 checked models; 0.997513x preceding candidate: neutral, one wave. |
| [5389 publication B28+C5](benchmarks/runs/20260907-5389-dense-publication-BC-gate/summary.json) | 66/66 checked/clean; no timeout/solve loss; candidate/production 0.950318x. |
| [5392 coupled witnesses](benchmarks/runs/20260907-5392-eliminate-entry-runtime/summary.json) | 12 checked models; 1.020252x preceding candidate, 1.205145x Kissat; mixed, one wave. |
| [5393 coupled B28+C5](benchmarks/runs/20260907-5393-eliminate-entry-BC-gate/summary.json) | 66/66 checked/clean; no timeout/solve loss; candidate/production 0.922328x. |
| [5395 coupled Rank 1–40](benchmarks/runs/20260907-5395-eliminate-entry-ranks1-40/summary.json) | 80/80 checked/clean; both 40 solves; 8378.399/6762.780 s = 1.238899x. |
| [5422 Rephase B28+C5](benchmarks/runs/20260907-5422-rephase-root-BC-gate/summary.json) | 66/66 checked/clean, no solve loss/timeout; candidate/production 0.936764x. |

Rank 1–40 is not an annual/union Tier-D replacement. Memory competitiveness
is an independent acceptance dimension, not discharged by a runtime ratio.

## Exact next action

- Exact next action: Advance the isolated [7144 compact Vivify pilot](benchmarks/reports/2026-09-10-vivify-compact-implementation-7151.md), approved by the user and implemented from frozen7030, not rejected7116. New canonical homogeneous paths collect refs without literal snapshots: redundant arena pose/ref sorting; ordinary irredundant stable radix with logical-position ties, separate skip-sort regime. Const/mixed/nonreuse/flush fallbacks, two-pass root/count timing, pass-time glue membership, priority, pose, ticks, proofs and N1–N4 unchanged. Full suites115/115,115/115,114/114 PASS;96 ordering digest c67723e59482dad7 and1728 transaction/model/proof-digest cases exact frozen7030 Release/trace (11603ba2e1d600bb). Six Debug ASan/UBSan/LSan fixtures twice PASS;96 independent DRAT/LRAT proofs and192 negatives PASS/exact parent. Frozen7144 build prose miscounts1728 as3456; successor evidence explicitly corrects it, no inflated coverage. Release/BC SHA cdcb735715d4eb2c23d43bbff14cb9d5ccedaa41592aab693cdc779b531dbdcf; trace f39157f4139cbc75ac5ba0ef83f01cab445f9e855970dc89780c3d2bafef7921. Production c2a95a6 unchanged;7144 unpromoted/unfuzzed. 7149 B28+C5 job1680134 since2026-09-10T21:19:24 local, watcher94244: all66 expected, all33 work/proof pairs exact,64 checked at immutable neutrality-7149 checkpoint; last proofs asynchronous. 7150 runtime wave0 job1680200 since21:26:12 local; run subsequent waves1/2 only after exact preceding solver outcomes, never wait for proof checking.7152 profile preparer/validator/submitter ready, unsubmitted: first require7150 Rank35 tasks2/3 exact outcomes, then three full-width paired profiles. Candidate run_phase call PCs507a93/508343/508ee2 statically reviewed; frozen7030 control PCs unchanged. Preserve15% complete-Vivify Rank35 sampled-cycle reduction plus1% whole-runtime median reduction on BOTH31/35; no runtime/RSS regression31/35/38, three repetitions, unchanged variance rule. No timing benefit established yet. [7151 audit](benchmarks/reports/2026-09-10-vivify-compact-audit-7151.json) owns exact published states/SLURM/validation; old7125 runtime and7128 profiles now terminal with final clean proof records to audit. Only7148 fixed-control inputs/certificates179234B removed with receipts, no old campaign cleanup. 7135/7140 diagnoses and7143 controls remain frozen;7116 BVE failed20% complete-Eliminate target and RSS31/35 gate, no further tuning/promotion/full50 refresh. [7098 checked50](benchmarks/reports/2026-09-10-collect-retry-checked50-7098/comparison.md) remains all150 checked-clean,8130.686/7239.010=1.123176512x,819.2859s still needed; memory/full50-transfer/consolidation/current-source fuzz remain open. Preserve4/32/33/49/46 closures, shelve21, leave7077 unsubmitted. Direct Codex debugging; no Claude delegation unless user asks. Claude-owned fuzz unavailable/uninspected; no denied transfer/push retry. Updated revamp communicates mini-designs and proceeds with in-scope rewrites/reassessment without routine approval pauses; genuine authorization boundaries and gates remain. Andrew report-to-andrew/20260910-2126-vivify-compact/report.pdf local/unsent. Never rerun successful7144 preparer/7145 builder/7146/7147/7148 writers or7149 seal/launch; preserve measured source and pins. Prior accepted frontier/history below remains frozen,not instructions to resume shelved probes. 6798/job1678834 terminalCOMPLETED;strict6800 PASSED bothclean/exactwhole6794 traces/state/partialproof/nativeold10Kproof. Group6803 counts15774/15784requests,1187groupsboth,onlysixdifferent:23987/23988/24045/24060 current402386insteadnative402382;25095/25096 nativealone402381,402380,402433,402378,402428.402380firstthreebumpsmatch;twoextranativemovesproduce1827240vsretained1824463. Firstrawlinkdifference23973,next402385vs402375for402361,retained/unexplained;notautomaticallyharmful. Prioritizeearliestcauseofworsework,notrawidentityparity. Sixnewgroupingtests;232bench/19slurmPASS;existingparsersfrozen. Same6730/native6090 and6794runner+existingQUEUE_BUMP only,C23890..25102,lastinprocessingthroughsplit,no globaltrace/secondstop. Exactwhole6794 decisions/prefixes/partialproof/boundaries/macro/state/nativeold10Kproof mandatory;only11-columnbumpsadded,rawlinks/stamps/duplicatespreserved.6794/job1678806 strict6796 PASSED bothclean50K/exactwhole6751 replay,29/30staterowsmatchdecisions. AtD25451309 bothchoose-440263/L13,402380unassigned/heap304056/score6;stamp1824463/1827240,prev402381both,next421409/402408;globalqueue1827409/1827419. AtD25451310 currentchooses-402373while402380stillunassigned,native-402380. Currentlaterbinary:-402372 assignmentisdownstream,notaskipcause. Tracebumpmembership/order/incomingstate,notselector/phase/scorehack. Existingfivequeue/sevenstateparsertestscoverunchangedconfiguration;prior226bench/19watchertestsPASS. 6782/job1678801 strict6784 PASSED bothclean50K/exactownreplay,341/252uncappedrows.6793 allfivepre-splitconflict/analysis transactions25098..25102matchafterexistingphysical-IDnormalization;allfourlearnedstagedigests matchallsixtransactions. FirstchoiceC25102/D25451310/L14 -402373/-402380 remains;earlierlatentqueue/assignmentcauseopen. 6757/6773 classified candidate1200s timeout/exact available decisions, native preprocessing250000-row cap/no selected detail; strict6759 preserved.6774/6781 classified competing50001 event-stop preempting deferred50000 prefix flush,both prefixes missing/wrappersFAILED1:0;both partialproofsexact, candidatewholedecisions/nativeavailableprefixexact;strict6776 preserved,neitherfailedtraceaccepted. Original6751/6753 hasboth clean50K stops;first25946740 choicesmatch thenC25102/D25451310/L14 current-402373/native-402380;earlierSweep42-binary residual notassumedcausal.6763 onlymoves historicalmarker intooldpadding;6765 restoresKernel7864/watches352/marker412/nextptr416,allmeasured6654offsets.112/112/111suites,288watermarkstates,and6769eightDRAT/LRATfixedproofs/root/model/reasoncontrols allPASS/exact6730. BC6766/6768/job1678721 since22:23:55 all66expected/all33pairsexact,946.177/946.376s neutral,64checked snapshot.6770/6772 threewaves1678787/1678793/1678797 since22:32:29/22:39:21/22:47:08 all12expected/exactwork/proof,8checked snapshot;Rank38pairedmedian0.999842727x,Rank46 1.000756248x,allspreads<0.31%:rejectlayoutcosthypothesis,no full50for6763.6754/6756 threeexactworkRank38pairs stillprove1.058875784x6730/6654 slowdown,all6checked;firstpairinstructions1.000024059x/cycles1.060601343x at3.800GHz/fullCPU.6787 Rank38 same-pin profile/job1678803 since23:16:02 watcher41747;strict6789 bothUNSAT/exactfullstats/proof/zero lost samples,0/2checked snapshot. Mainpropagationself68.00%/65.87%;6790 offline933offsetsexact,periods928443010744/848015261323=1.094842337x,largestdeltaat+840 binary/largebranchaftervalueload;sampling/skidnotintrinsicinstructioncost,nocodeplacementcauseproven. Onlyown1183320440B temporaryperfexpansionsremoved,compressedoriginalsretained.6777/6778 kernel_propagate4129bytes/933rows;6763vs6654 everyrelativeinstruction/nonrelocatedoperandexact,10samesymbolcalls/2equalreadonlywords;address0x413730vs0x413790:6797 unchanged-sourcewatchrebuild starts0x410c60butshrinks96B inboth6730/6763;threefollowingpropagationfunctionskeepsizes/move-96B. Staticdisplacementexplained,notruntimecause/noalignmenttuning. Frozen6750 all50expected25SAT25UNSAT,no loss/timeout,8300.164/7239.010=1.146588277xfixedKissat;parent8240.560,+59.604sadverse,other49samework+123.911s/37slower;maxRSS15.272984GiB,notmemoryparity. Full6743/job1678659 terminalall50checked/clean;task42proofVERIFIED10070.005s,finished00:27:56EDT; prior6666 all50checked,BC6737 all66checked;successoraudit6779ownsfinalpublishedstates. Rank46retained6745/6747 all6checked,median0.563096874xparent,83.200/80.043native=1.0394413x;fullC3082376/D5883417exactnative.6739all2167922choices through1M/117watchrowsmatchnative;6723/6729historicalReducecutthroughpromotion,notcurrentredminimum. PreserveRank4/32,strongerRank33closure6519,Rank49closure6663/6672 (all6checked,0.142716734xparent/1.0043006xnative),unit-onlylazycleanup,compactstamp6357/activation6424,N1–N4/SP-SLOT-5,rejectedsharedblocker6448/preclean5920,6642legitimateRank35deletion. Source/pins6730manifest;productionc2a95a6unchanged. Checksasync/publishedonly;invaliditypausesdependents. Memoryopen;FlatListsunapproved;Claude-ownedfuzzunavailable,noartifactaccess/transfer/pushretry/promotion. Fixed41–50/6108,fresh6121/6227,cpu-q5784immutable.Prior2cd1b76d/6785 retains3rawasmunchanged/hash-boundlocallyafterwhitespacecheckcaughtemittedspaces;no gateweakening. Andrew local report-to-andrew/20260910-0058-vivify-snapshot/report.pdf,unsent;audit6836/landing6837ownsnewcheckpoint;prior510aedd0/6818/6821frozen,8rawprofilesremainunchanged/hash-boundlocally;only6827ownedfixedinputs/proofsremoved. Prior44e99b1d/6801 remainsfrozen. Localmonitor81311confirmedlive;deadlinefixedSep10evenaftermidnight.
   5925/5929 prove full parent5900 trace/proof replay through300K;5926 Rank32/37/21 proof/non-timing stats also replay5900. The three-clause second-forward count gap isnot first semantic drift. Validated5917 shows equal shared scores/flags but1611 displaced positions and17 one-sided members;several identities also occur in5931 differing ternaries. No equivalence orheap-tie cause inferred. Validated5910 separate5870 Rank31 hasonlynine zero-score positions different;prioritize current5900 gaps.
   Failed5862/job1674504 standalone schedule observer exits before required300K prefix;no solver crash. Separately validated109255-member schedule has six score residuals exactly explained by nine free occurrences in three incoming-root-satisfied clauses.5870 deletes those before reconnect,not after;later-derived lazy occurrences are deliberately retained. No full formula/trajectory identity claim.
   Parent5849 full5858 remains1.136184x,40/40 checked;BC5855 B0.746020x/C0.928639x,66/66 checked. Parent5798 full5811 now40/40 checked1.127044x;5836 BC66/66 checked;5838 full39/40. Preserve original solver-time snapshots and parent C regression at5806 (1.076240x).5945/job1675510 intentionally CANCELLED before interpretation;bothscratchclean. Corrected5948 observer passes actual pre-initial construction regression,then5949 complete replay.
   Fuzz5765/5777 external-transfer retry denied;explicit approval required,no retry or fuzz-artifact inspection. FullTierD5784/job1673402 cpu-q remains2332 tasks,4CPU/32GiB/1200s solver/5h allocation. Tasks828/829 and1280/1281 have separately reproduced empty-input checker-status false negatives;allfour remain invalid/unscored under unchanged runner. Second inputemptyclause24207 andpositive/negative controls verified5942;separate audited repair required. All old pins/launches immutable.
   Full5712 now80/80 checked/clean,1.110877xKissat;summary sealed5714.5733 selects31/33/37. Never rerun5713/5665 snapshots.5664=1.128369x,latest79/80 checked;seal5666.
   Do not rerun frozen5608 snapshot. Timing is replicated changed-work gain,not isolated unit cost.
   Parent5535 remains1.162180x,all80 checked/clean;summary sealed;do not rewrite its snapshot.
   Parent5531 is6/6 checked,median0.889407x5510.5522 Release/BC SHA67867f4b999b55b4… .
   5585/5587 identify the extra five-literal candidate;5588 proves its sole binary support
   reaches a root-fixed smaller endpoint.5592 fixes that count,not physical binaries or the heap.
   Do not run old5734 paired-full helper. fixed_runtime.py has11 protocol tests including final timeout versus rejected certificate;full40 snapshot stays frozen.5679 all6 checked/noisy;5678 BC66/66 checked,combined0.934484x production.
   `compare-rank32-sweep-5503.py` validates5490's complete observed parent replay;
   `sweep-localization.json` splits both calls at19375. First14 seeds, environment
   sizes and effort deltas/solve counts match; then Tactix alone processes836.
   First-call native-only unit-103918 is later, not a proved effect of that skip.
   Original5490 fails an observer expectation: retained Tactix emits no fresh
   schedule, while four native event families emit before the selected window.
   Successor5503 records those separately with ten window fixtures; no solver failure.
   Square5502 has1555 extensions per arm and full parent replay. First unequal
   accepted grid is943: Tactix seed-4143/member-4147 versus native-4171/-4175.
   Investigate incoming physical formula and Factor heap before a repair.
   Definition5498 passes successor5501 (twelve framing fixtures): auxiliary43773
   has different defining clauses.5496/5497 missing snapshots and5498's lucky
   payload expectation remain frozen failures; no failed correctness oracle.
   Square 5482 passes successor `validate-square-phase-5486.py`: both complete
   streams/prefixes agree with their parent; native adds 476 analysis/end rows,
   while all 986 original boundaries match after sequence normalization.
   The original strict comparator is retained as a failed observer expectation.
   Both phase picks use saved state; Tactix saves negative43773 at conflict492.
   Earlier assignment histories differ too; do not change phase priority.
   Rank 31's whole 100K prefix and portable boundaries agree; its full-run loss
   needs a later trace after the current Rank32/37 component investigations.
   5475 now has three complete statistics/proof-byte identities: median
   0.984563x predecessor, all pairs improve and both arm spreads below 0.5%.
   All six5475 proofs are now checked/clean; its final summary is sealed.
   Both5469 and5476 B28+C5 gates are sealed66/66, no solve loss/timeouts.
   Full5485's solver snapshot
   is frozen:1.165337x,59/80 checked; never rerun its snapshot helper.
   `next-losses.json` from5500 keeps Ranks37/31/32 as largest losses and verifies
   all40 candidate non-timing statistics/certificates unchanged from5459.
   Final checking uses `seal-sweep-full-5485.py`.
   Predecessor source `.claude/worktrees/runtime-sweep-cursor-5473` and Release
   5473/5474 share SHA `305b2285fdda5834…`; its manifest owns exact identities.
   5468's three pairs pass exact statistics/proof-byte identity: median
   0.983980x predecessor; all paired ratios below 1, both arm spreads below 2%.
   All six append timing proofs are checked/clean in 5468's final summary.
   Retain the supported optimization provisionally; B28+C5 is checked and
   latest-source fuzz remains pending. Do not repeat timing for confirmation.
   5465's final summary is sealed:1.173817x, all80 expected verdicts checked/clean.
   Its `next-losses.json` selects Ranks 37/32/31. Completed 5478 first splits at
   conflict188502; `boundary-localization.json` first finds two Sweep binaries
   at152381, while `nonbinary-localization.json` finds one retained Vivify clause
   at188501. Neither is a proved cause or accepted neutral difference.
   Watcher30072 is terminal. The 5478 matching-row count is suffix-only;
   5463 separately proves the complete earlier 35,346,388-row common prefix.
   5458 confirms native bound advancement after the cut; 5463 closes the
   100000-conflict prefix, including all 272 portable boundaries. First-call 5457 grids
   are equal by member ordinal; raw observer emission order is not a mismatch.
   5462's short cases have identical old/new work/proof/slot traces; they do
   not reproduce the repaired cut. Square 5471 instead locates a phase split
   after 5848 common decisions, before those later inprocessing differences.
   Short diagnostics do not replace the fixed regression gates.
   Preceding source `.claude/worktrees/runtime-bound-advance-5459` is frozen;
   Release 5459/BC 5461 share SHA `84300eafeef6c973…`, trace 5460 `509877b2…`.
   Its manifest owns full hashes, tests and patches. Watchers 50218 (5463) and
   18033 (5458) completed. Full 5459 timing does not establish an aggregate gain;
   Rank-37 full work regresses despite its bounded causal closure. Retain the
   source-backed bound repair while localizing the later coupled mechanism.
   5453 is terminal and compared: 7,918,162 matching rows (was 7,504,348).
   `boundary-localization.json` places the next coarse split at the second
   Factor call, conflict 32,073: common input active 148610/binary 181122;
   output Tactix active 148613/binary 181013, Kissat 148618/180960. This
   Factor admission difference is now closed by 5459;
   coarse counts alone were not its proof. Inspect full 5454 work,
   runtime and memory, and seal 5454/5455 with `seal-eliminate-limit-gates-5455.py`.
   5454's complete solver view is 1.170283x. `next-losses.json` selects Rank 31
   (+154.211 s, conflicts 1.542264x) while Rank 37's work drops materially.
   Rank-31 comparator 5470 and boundary localizer 5483 now establish no split
   through 100K; watcher 4656 is terminal (COMPLETED).
   No causal timing attribution from different campaign windows.
   5437's unique binary sets agree: two physical duplicate copies explain the
   count split. Do not restore duplicates without evidence of a causal defect.
   Vivify 5440's small exact-work 0.995069x median gain now has six checked proofs.
   5447 now has three complete exact-work/proof-byte pairs: median 0.976662x
   predecessor (range 0.976171–0.989633), now all six proofs checked.
   Metadata 5426 is checked but remains independent/unfuzzed.
2. Revisit active checker outcomes without blocking the investigation; run
   `seal-rephase-gates-5422.py` and `seal-root-followup-gates-5435.py` when complete.
   `seal-rephase-replicas-5421.py` has sealed all nine timing proofs.
   Preserve per-instance work/time/memory and label verification status.
   Any rejected proof/model pauses dependent work under plan §8.2.
3. Preserve submitted sources/pins. Earlier phase-entry source is
   `.claude/worktrees/runtime-inprocess-root-5430`; Release
   `build/pin-5430-inprocess-root-release` and independent
   `build/pin-5432-inprocess-root-bc` share SHA-256
   `44cac3efc60149ab78dca2a5e6b17e552f2f860d4797226d6a0889ae93c1fe26`.
   5430 owns patches/tests; trace pin 5431. Separate metadata source/pins belong
   to 5426 (Release 5426/5427, SHA `bb4e3fbc…`); both descend from Rephase 5417.
   Private maximum-selection source `.claude/worktrees/runtime-vivify-max-5438`
   descends from 5430, excludes metadata, and owns Release 5438/5439 (matching
   SHA `2e316cb494cc10e24…`); its manifest owns full hashes, patch and tests.
   Predecessor `.claude/worktrees/runtime-replacement-cursor-5445` descends
   from 5438; builds 5445/5446 share SHA `b92994ec05c61c31…`. No submitted
   source may be edited. Its 840-case fixture preserves physical/derived state.
   Predecessor `.claude/worktrees/runtime-eliminate-limit-5450` adds only the private
   phase-budget stop and 12-scenario fixture. Release 5450/5452 share SHA
   `c7f57cbcb92f2611…`; trace 5451. Its manifest owns full hashes and patches.
   Claude's terminal fixture review passed: the initial expected elimination
   count was wrong at equality, not a solver defect; original failure retained.
4. Continue periodic Claude-owned fuzzing after meaningful implementation batches,
   in the background as requested.5765 current5741 owner resume awaits network-transfer approval;5715/4746 terminalPASS5672;5657 terminalPASS5649.
   5610:50000 cases,45752 checked verdicts,144 direct reachability cases,0failures.
   No live parent CLI for5765:38524 ended interim,96672 failed before inference. Do not repoll terminal handles or independently inspect fuzz jobs/artifacts;explicit external-transfer approval is required for the denied resume.
   This is a changed-source checkpoint, not duplicate confirmation. Require all
   runtime/B28+C5/fuzz gates. A reported soundness/crash/proof/model failure
   pauses dependent work and invokes bounded Claude repair immediately.
5. Keep broad cleanup separate. Preserve proof-before-deletion, native physical
   duplicates, N1–N4 and open ledger obligations. Push remains **unapproved**
   after the earlier auto-review rejection; do not retry or bypass it.
   Approval requested for separate future solve/check jobs to release solver
   allocations sooner; no contract/runner change until explicitly approved.
   SLURM accounting recovered at the latest5506 audit; controller queries work.
   Preserve unknown purged-job states and never restart jobs on observation failure.

## Immutable evidence and validation

- The runtime review routes historical campaigns. The
  [Vivify report](benchmarks/reports/2026-09-07-vivify-retry-repair.md) owns
  5377–5383, predecessor links, all 2107 BVE attempts and exact
  1,928,902 decisions through one million conflicts, not the full trajectory.
- The dense-entry report owns 5384–5395, both physical-mode repairs, profiles,
  measured memory ownership, initial fixture failure and Claude's terminal
  fixture-only repair. Final tests: 76/76 trace, 77/77 in each Release build.
  No new fuzz was launched or inspected; the candidate remains unfuzzed.
- The [heap report](benchmarks/reports/2026-09-07-rank35-heap-localization.md) owns 5396–5405, validated stream/window and direct heap
  observers, exact prefix/heap witnesses and cancellation cleanup. Both new
  trace builds pass 76/76 fixed tests; production source remains unchanged.
- Production passes 77/77 CTests. Claude's authoritative
  [r2 fuzz report](benchmarks/reports/2026-09-07-sweep-pose-fuzz.md) is PASS;
  regressions 028–030 remain. This does not cover isolated candidates.
- N4 verification remains pending. Historical accepted source `bad5cbf` and
  `20260829-1930-tierd-ranks1-40-accepted-fresh` are not same-window baselines.
  Frozen references: `benchmarks/baselines/sc24-26-32g-5000s/`. T1/T2 and
  historical T3 passed; T4-K/T4-VBS/subsequent T5 remain open.
- Raw large streams stay local with hashes; small patches, runners, comparators
  and witnesses are durable. Legacy prefix `start_decisions` wraps with warm-up
  counts and must never be used for alignment.

## Architectural contract

<!-- track-sched:begin -->
- Kissat creation order is the unconditional production default;
  `TACTIX_ARENA_ORDER=1` is an explicit CaDiCaL diagnostic arm only.
- Unsupported formula-dependent profiles stay removed; retain sound general
  repairs through downstream regression, not compensating behavior.
- Preserve N1 universal API, N2 proofs/models, N3 runtime/memory and
  N4 reference-strength scheduling.
<!-- track-sched:end -->

## Recent changes

- Historical Reduce watermark6730 repairs promoted-clause cursor andall2167922native decisions through1M onRank46;288regressions,112/112/111suites,8proofsPASS. Three-wave runtime/full50 gates active. Evidence: `benchmarks/reports/2026-09-09-rank46-reduce-watermark.md`.
- Parent5798 full5811 is1.127044x fixedKissat;Rank32 largest loss with2.510381x conflicts. Rank31 continuation recovered;new fixed native/Tactix caller witnesses expose missing dense closure before BVE.
- Current5741 preserves entire parent Rank31/33 traces and partial proofs;5754 BC66/66 checked,combined0.934250x production.5712 full and5727 BC also sealed;5679 remains noisy,not a proven unit-cost gain.
- Benchmark tools22 testsPASS;chunked complete-row comparison covers all chunk boundaries. Neutral5822 proves actual Rank32 root-mark provenance;5813/5834 add interruption and direct-conflict proof coverage. Paired TierD5784 remains immutable;fuzz owner resume still needs approval.
- Production unchanged; goal, full checks and cumulative promotion remain open.

Git and primary reports own history; prior resume text is in `a336e013:PROGRESS.md`.
