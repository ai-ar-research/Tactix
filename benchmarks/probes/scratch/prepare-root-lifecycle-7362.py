#!/usr/bin/env python3
"""Use existing root-assignment and deactivation observers on immutable pins."""
from pathlib import Path
base = Path(__file__).with_name('prepare-root-queue-7358.py')
code = base.read_text()
code = code.replace('7358-root-queue', '7362-root-lifecycle').replace('7356-bve-heap', '7358-root-queue')
code = code.replace('check-submit-root-queue-7358.py', 'check-submit-root-lifecycle-7362.py')
code = code.replace('2026-09-11-bve-heap-analysis-7357.json', '2026-09-11-root-queue-analysis-7359.json')
code = code.replace("('decisions', 'macro', 'active', 'inprocess', 'heap', 'queue')", "('decisions', 'macro', 'active', 'inprocess', 'heap', 'queue', 'roots', 'deact')")
code = code.replace("('decisions', 'macro', 'active', 'inprocess', 'heap')", "('decisions', 'macro', 'active', 'inprocess', 'heap', 'queue')")
before = "        os.environ['TACTIX_QUEUE_STAMP_TRACE'] = str(trace_files['queue'])\\n        solve = measured(command, stdout, stderr,"
after = "        os.environ['TACTIX_EVENT_TRACE'] = str(trace_files['roots'])\\n        os.environ['TACTIX_DEACT_TRACE'] = str(trace_files['deact'])\\n        os.environ['TACTIX_EVENT_TRACE_HEAP_VARS_ONLY'] = '1'\\n        os.environ['SAT_EVENT_TRACE_HEAP_VARS'] = '1333,1334,1335,1337,1338,1340,1343,1344,1345,1346,1347,1349,1350,1351,1352,1353,1354,1356,1357,1358,1359,1360,1361,1362,1363,1364,1365,1366,1367'\\n        os.environ['TACTIX_EVENT_TRACE_CONFLICT_FROM'] = '0'\\n        os.environ['TACTIX_EVENT_TRACE_CONFLICT_TO'] = '14492'\\n        os.environ['TACTIX_EVENT_TRACE_LIMIT'] = '1000000'\\n        solve = measured(command, stdout, stderr,"
assert code.count(before) == 1
code = code.replace(before, after)
before = "        del os.environ['TACTIX_QUEUE_STAMP_TRACE']\\n        verdict, statistics = solver_summary(stdout)"
after = "        for name in ('TACTIX_EVENT_TRACE','TACTIX_DEACT_TRACE','TACTIX_EVENT_TRACE_HEAP_VARS_ONLY','SAT_EVENT_TRACE_HEAP_VARS','TACTIX_EVENT_TRACE_CONFLICT_FROM','TACTIX_EVENT_TRACE_CONFLICT_TO','TACTIX_EVENT_TRACE_LIMIT'): del os.environ[name]\\n        verdict, statistics = solver_summary(stdout)"
assert code.count(before) == 1
code = code.replace(before, after)
code = code.replace("schema='tactix-root-queue-7358-v1'", "schema='tactix-root-lifecycle-7362-v1'")
start = code.index("    hypothesis=")
end = code.index("m['pin_sha256']", start)
code = code[:start] + '''    hypothesis='Identify when the29 root-prefix variables dropped by compaction become fixed and whether their transitions are consumed by guidance. Existing selected-variable root assignment events (including warming/reason) plus complete deactivation sink; selected conflict window0..14492, no solver stop or source edit.',
    acceptance='Both full solves must replay own Release work/certificate and all six entire7358 archives before interpreting the new roots/deactivation traces. Independent original proofs remain asynchronous; pending means provisional, not accepted. No runtime claim.',
    test_consideration='Existing test_event_trace covers selected root assignment emission. Parser must validate every emitted root row and deactivation schema, retain negative events/absences only within complete covered window, and not conflate first fixed assignment with the later compaction. Add actual-consumer unit regression at the identified owner before repair.')
''' + code[end:]
exec(compile(code, str(base), 'exec'), dict(__file__=str(Path(__file__)), __name__='__main__'))
