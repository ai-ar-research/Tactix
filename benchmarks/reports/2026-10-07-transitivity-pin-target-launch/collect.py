#!/usr/bin/env python3
"""Read-only paired grading after both full-width tasks have terminal artifacts."""
import json,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
paths=[HERE/'results'/f'{sys.argv[1]}-{index}'/'result.json' for index in (0,1)]
records=[json.loads(path.read_text()) for path in paths]
off,on=records
matched=(off['manifest_sha256']==on['manifest_sha256'] and len(off['common_startup'])==4 and off['common_startup']==on['common_startup'] and off['publication_valid'] and on['publication_valid'])
accepted=[bool(matched and record['solved']) for record in records]
use=bool(on.get('terminal') and on['terminal'].get('native_unit_uses',0)+on['terminal'].get('native_conflict_uses',0)>0)
cdcl=[any(record.get('record')=='canonical-solve-return' and record.get('decisions_delta',0)>0 for record in arm['records']) for arm in records]
result={'matched_context_valid':matched,'independently_accepted_off':accepted[0],'independently_accepted_on':accepted[1],
        'on_target_recovery':bool(accepted[1] and use and cdcl[1]),'on_accepted_terminal_without_consumption_claim':bool(accepted[1] and not (use and cdcl[1])),'terminal_native_consumption_observed':use,
        'historical_consumption_known':False,'continuing_cdcl_observed':cdcl,
        'solver_process_wall_seconds':[record['process']['wall_seconds'] for record in records],
        'checker_wall_seconds':[record.get('certificate',{}).get('wall_seconds') for record in records],
        'paired_runtime_ratio_on_over_off':on['process']['wall_seconds']/off['process']['wall_seconds'] if all(accepted) else None,
        'formal_multitarget_benchmark_claim':False,'default_promotion':False}
print(json.dumps(result,indent=2))
