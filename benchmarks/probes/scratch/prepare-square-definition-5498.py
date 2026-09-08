#!/usr/bin/env python3
"""Native startup snapshots use factor_end events, not named slot payloads."""
from pathlib import Path
source = Path(__file__).with_name('prepare-square-definition-5497.py').read_text()
source = source.replace('20260908-5496-square-definition', '20260908-5497-square-definition-phase')
source = source.replace("c = root / 'benchmarks/runs/20260908-5497-square-definition-phase'",
                        "c = root / 'benchmarks/runs/20260908-5498-square-definition-selectors'")
source = source.replace("'tactix-5496-'", "'tactix-5497-'").replace("\"'tactix-5497-'\")", "\"'tactix-5498-'\")")
anchor = "assert '_EVENT_TRACE_IMPLY_ONLY' not in runner"
source = source.replace(anchor, '''runner = runner.replace("'SAT_EVENT_TRACE_BINARY_SET_MATCH': 'stage=end'",
                        "'SAT_EVENT_TRACE_BINARY_SET_MATCH': ('_end' if arm == 'kissat' else 'stage=end')")
runner = runner.replace("'name=' + ('factor' if arm == 'kissat' else 'Factor') + '|stage=end'",
                        "('factor_end' if arm == 'kissat' else 'name=Factor|stage=end')")
''' + anchor)
source = source.replace("observer_failure='5496 has two clean stops and full replay, but IMPLY_ONLY returns false from want_detail/detail_enabled, suppressing slot snapshots. Preserve its missing-snapshot comparator failure. Reuse PHASE_ONLY from validated5482; no binary/source edit.'",
                        "observer_failure='5496 suppressed detail hooks;5497 captured Tactix but not native because native probe.c emits factor_end with payload -, not name=factor|stage=end. Both failed probes have full per-arm replay and are retained. Correct native binary _end and formula factor_end selectors; no source/pin edit.'")
exec(compile(source, __file__, 'exec'))
