"""Maintained real-client trace corpus, including independent certificate checks."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

driver,root=map(Path,sys.argv[1:])
for name in ('veripb','cake-pb-cnf','drat-trim','cake_lpr','dsr-trim','dsr-compress','srcheck'):
    if not (root/'tools/checkers/bin'/name).is_file():
        print('IPASIR trace regression: missing checker',name)
        sys.exit(77)
with tempfile.TemporaryDirectory(prefix='ipasir-trace-regression-') as tmp:
    output=Path(tmp)/'results'
    command=[sys.executable,str(root/'tools/fuzz/ipasir_trace.py'),'--driver',str(driver),
             '--corpus',str(root/'tools/fuzz/ipasir-corpus'),'--out',str(output),'--root',str(root)]
    reference=root/'cadical/build/cadical'
    if reference.is_file():command+=['--reference',str(reference)]
    # The retained minimization trace adds independently checked probe queries;
    # each child retains its 30-second limit, while the complete corpus grows.
    result=subprocess.run(command,capture_output=True,text=True,timeout=300)
    assert result.returncode==0,(result.stdout,result.stderr)
    summary=json.loads((output/'summary.json').read_text())
    assert summary['status']=='PASS'
    for name in ('10-to-20','20-to-10','add-after-10','add-after-20','opposite-assumption',
                 'duplicate-assumption','multiple-live-sessions','proof-verified','model-verified','fresh-query',
                 'termination-polls','unknown-validated','0-to-10','strategy-switch',
                 'strategy-calls','strategy-1-solve','strategy-2-solve','strategy-3-solve',
                 'learned-clauses','learned-units','learned-empty','reduced-core-verified','owned-result-verified',
                 'strategy-4-solve','strategy-8-solve','native-root-requests','native-root-eligible','native-root-effective',
                 'native-root-rounds','native-handoffs','strategy-5-solve',
                 'random-root','random-search','random-root_to_search','random-partial_restarts',
                 'random-full_restarts','random-search_to_root','random-algebraic',
                 'random-effective-rule-BVE','random-effective-rule-BCE',
                 'random-effective-form-canonical','random-effective-form-instance',
                 'random-effective-form-semantic','random-effective-form-online',
                 'minimizations','min-context','min-strict-reduction','min-complete','min-incomplete',
                 'min-zero-budget','min-hard-unsat','min-context-unsat','min-witnesses',
                 'min-strategy-calls','min-probe-result-10','min-probe-result-20',
                 'min-probe-result-0','min-termination-polls'):
        assert summary['coverage'].get(name,0)>0,(name,summary)
    assert 'IPASIR-UP' in summary['interfaces_excluded']
    print(result.stdout)
