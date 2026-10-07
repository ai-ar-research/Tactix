# Fixed-44 open SAT diagnostic: terminal result

The 24-instance paired screen `1754670` completed both arms on all inputs. Each task ran the unchanged baseline then the reference-policy candidate sequentially on the same CPU, with 600 seconds and a 2 GiB per-output-file limit per arm. All frozen wrapper and binary hashes were reverified. Candidate Release `10942fbe` binds production checkpoint `3ecfa8dfe`; this diagnostic does not qualify the complete fixed-44 portfolio.

| Arm | Independently checked SAT | Solver timeout | Storage-censored |
|---|---:|---:|---:|
| Baseline | 1 | 13 | 10 |
| Reference policy | 2 | 14 | 8 |

The candidate checks rank 401 in **80.526 seconds** and rank 991 in **29.959 seconds**. The baseline checks rank 991 in **29.658 seconds**. Rank 401's baseline is storage-censored at 541.967 seconds, so this screen alone cannot assign it a full-limit baseline loss. A separate matched 300-second causal experiment already records baseline timeout and independently checked candidate success.

There are no further checked candidate solves. Rank 97 changes from a baseline storage censor to a candidate timeout; that is not a solve or a reliable speed comparison. Complete-arm PAR-2 is withheld because capacity failures are incomplete evidence. Neither the 600-second timeouts nor the storage censors establish 5,000-second failure. Same-candidate retention, the >=12/44 milestone, full Debug, Claude-owned fuzz and all regression gates remain outstanding.

The selected policy changes initial orbit-cut selection and preserves the original search seed. For completed candidate timeouts, initial structured processing takes roughly 0.07–9.4 seconds, while several rows spend 96–230 seconds in the inprocessing slot loop. This rules out startup cost alone as the explanation for those measured timeouts; it does not prove that disabling inprocessing improves search. Rank 899 is separately undergoing an actual-Satsuma presearch comparison.

`summary.json` binds all 48 result records and preserves per-arm validation, timing, resource and measured-stage data. Raw campaign results and the independent original-CNF model checks remain authoritative. No solver or checker was rerun to produce this report.
