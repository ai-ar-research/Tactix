# SLURM scripts of the paper's runs

Each script is in `<experiment>/<campaign>/`.
`array` runs one task per (configuration, formula); `validate` and `aggregate`
check the campaign's records and collect them. `pins/X.py` is the campaign's
frozen copy of `tools/bench/X.py`; `runner/X.py` likewise.

| Experiment | Campaign | Script | Partition | Cores | Memory | Time | Array | Runs |
|---|---|---|---|---|---|---|---|---|
| tierd | 20261010-tierd-fastel | array | cpu-q | 4 | 32768M | 09:45:00 | 0-1165%125 | `pins/runtime_task.py` |
| tierd | 20261010-tierd-fastel | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| tierd | 20261011-tierd-fastel-recheck-64g | validate | cpu-q | 4 | 65536M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| tierd | 20261011-tierd-fastel-recheck-64g | array | cpu-q | 4 | 65536M | 09:45:00 | 0-6%125 | `pins/runtime_task.py` |
| tierd | sc24-26-32g-5000s | aggregate | cpu-dense-hwulab-q | 1 | 8G | 01:00:00 |  | `tools/bench/frozen_baseline.py --campaign` |
| tierd | sc24-26-32g-5000s | validate | cpu-dense-hwulab-q | 4 | 32G | 01:00:00 |  | `tools/bench/frozen_baseline.py` |
| tierd | sc24-26-32g-5000s | array | cpu-dense-hwulab-q | 4 | 32G | 14:15:00 | 0-5829%23 | `tools/bench/frozen_baseline.py --campaign` |
| tierd | sc24-26-32g-5000s | storage-audit | cpu-dense-hwulab-q | 1 | 1G | 00:05:00 |  |  |
| tierd | sc24-26-satsuma-fixed64-32g-8h | array | cpu-q | 4 | 32G | 10:15:00 | 0-1165%100 | `runner/baseline_rerun.py --campaign` |
| tierd | sc24-26-satsuma-fixed64-32g-8h | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `runner/baseline_rerun.py --campaign` |
| tierd | sc24-26-recheck-64g-8h | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `runner/baseline_rerun.py --campaign` |
| tierd | sc24-26-recheck-64g-8h | array | cpu-q | 4 | 64G | 10:15:00 | 0-64%25 | `runner/baseline_rerun.py --campaign` |
| tierd | sc24-26-satsuma-recheck-64g-8h | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `runner/baseline_rerun.py --campaign` |
| tierd | sc24-26-satsuma-recheck-64g-8h | array | cpu-q | 4 | 64G | 10:15:00 | 0-30%60 | `runner/baseline_rerun.py --campaign` |
| milestones | 20261011-tierd-noproof-1-tierA-dcc3ecbd6 | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| milestones | 20261011-tierd-noproof-1-tierA-dcc3ecbd6 | array | cpu-q | 4 | 32768M | 02:00:00 | 0-1165 | `pins/runtime_task.py` |
| milestones | 20261011-tierd-noproof-2-tierB14-546298924 | array | cpu-q | 4 | 32768M | 02:00:00 | 0-1165 | `pins/runtime_task.py` |
| milestones | 20261011-tierd-noproof-2-tierB14-546298924 | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| milestones | 20261011-tierd-noproof-3-tierC-6f44f23d6 | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| milestones | 20261011-tierd-noproof-3-tierC-6f44f23d6 | array | cpu-q | 4 | 32768M | 02:00:00 | 0-1165 | `pins/runtime_task.py` |
| milestones | 20261011-tierd-noproof-4-rank1-50-f50e46c0c | array | cpu-q | 4 | 32768M | 02:00:00 | 0-1165 | `pins/runtime_task.py` |
| milestones | 20261011-tierd-noproof-4-rank1-50-f50e46c0c | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| milestones | 20261011-tierd-noproof-5-wv100-0a9063387 | validate | cpu-q | 4 | 32768M | 00:05:00 |  | `pins/validate_anonymous_runtime.py` |
| milestones | 20261011-tierd-noproof-5-wv100-0a9063387 | array | cpu-q | 4 | 32768M | 02:00:00 | 0-1165 | `pins/runtime_task.py` |
| interface | 20261015-120822-A-tactix-public | aggregate | cpu-dense-hwulab-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| interface | 20261015-120822-A-tactix-public | array | cpu-dense-hwulab-q | 8 | 32G | 02:03:00 | 0-399 | `tools/bench/run.py --run-one` |
| interface | 20261015-public-port-v3-regression-b28c5 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-32 | `pins/runtime_task.py` |
| interface | 20261015-prod-samewindow-regression-b28c5 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-32 | `pins/runtime_task.py` |
| interface | 20261015-public-port-v3-regression-rank1-50 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-49 | `pins/runtime_task.py` |
| interface | 20261015-prod-samewindow-regression-rank1-50 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-49 | `pins/runtime_task.py` |
| interface | 20261015-public-port-v3-regression-wv100 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-99 | `pins/runtime_task.py` |
| interface | 20261015-prod-samewindow-regression-wv100 | array | cpu-dense-hwulab-q | 12 | 32768M | 01:10:00 | 0-99 | `pins/runtime_task.py` |
| cruxmiter | 20261010-213743-crux-fixed-family-verify | array | cpu-dense-hwulab-q | 12 | 48G | 168:02:00 | 0-29 | `tools/bench/run.py --run-one` |
| cruxmiter | 20261010-213743-crux-fixed-family-verify | aggregate | cpu-dense-hwulab-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-141329-chvatal-population-kissat | array | cpu-q | 2 | 8G | 13:55:20 | 0-888 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-141329-chvatal-population-kissat | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-144648-chvatal-band-baselines | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-144648-chvatal-band-baselines | array | cpu-q | 4 | 16G | 13:55:20 | 0-201 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-144649-chvatal-band-satsuma | array | cpu-q | 4 | 16G | 04:00:00 | 0-100 | `tools/bench/chvatal_satsuma.py --task` |
| chvatal | 20261015-145107-chvatal-band-candidates | array | cpu-q | 4 | 16G | 13:55:20 | 0-302 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-145107-chvatal-band-candidates | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-150949-chvatal-band-recheck48-1 | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-150949-chvatal-band-recheck48-1 | array | cpu-q | 12 | 48G | 13:55:20 | 0-42 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-160036-chvatal-hard-recheck48-2 | array | cpu-q | 12 | 48G | 13:55:20 | 0-16 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-160036-chvatal-hard-recheck48-2 | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-171709-chvatal-hard-recheck48-3 | aggregate | cpu-q | 1 | 8G | 01:00:00 |  | `tools/bench/run.py --aggregate` |
| chvatal | 20261015-171709-chvatal-hard-recheck48-3 | array | cpu-q | 12 | 48G | 13:55:20 | 0-2 | `tools/bench/run.py --run-one` |
| chvatal | 20261015-173818-chvatal-hard-satsuma-recheck64 | array | cpu-q | 16 | 64G | 04:00:00 | 0-0 | `tools/bench/chvatal_satsuma.py --task` |
