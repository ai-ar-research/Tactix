#!/usr/bin/env python3
"""One SLURM-only matched public-action mechanism diagnostic, not performance."""
import gzip,hashlib,json,os,subprocess,time
from pathlib import Path
from checker_route import AnonymousScratch, check_files, invoke, prefix, run_controls, sha
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[3]
def main():
    if not os.environ.get('SLURM_JOB_ID'):raise RuntimeError('SLURM mandatory for actual target actions')
    manifest=json.loads((HERE/'manifest.json').read_text())
    if not manifest['ready']:raise RuntimeError('unbound preparation package')
    destination=HERE/'results'/os.environ['SLURM_JOB_ID'];destination.mkdir(parents=True,exist_ok=False)
    scratch=AnonymousScratch('/tmp');results=[];controls=[]
    try:
        for binding in manifest['bindings']:
            assert sha(binding['path'])==binding['sha256'],'source/object/header/checker binding changed'
        for name,expected in manifest['source_files'].items():assert sha(HERE/name)==expected,'pilot source changed'
        inp=manifest['input'];compressed=ROOT/inp['path'];assert sha(compressed)==inp['sha256']
        controls=run_controls(scratch,manifest['checker_pins'])
        cnf=scratch/'original.cnf'
        with cnf.open('wb') as out:decompressed=invoke(['xz','-dc',compressed],30,out)
        assert decompressed.get('exit')==0 and cnf.stat().st_size==inp['decoded_bytes'],'full original input decode'
        cnf_sha=sha(cnf)
        common=None
        for arm in manifest['arms']:
            stdout=scratch/f'{arm}.stdout';proof=scratch/f'{arm}.dsr';provenance=scratch/f'{arm}.provenance'
            with stdout.open('wb') as out:
                process=invoke(['timeout','--signal=TERM','--kill-after=1','30',manifest['driver_pin']['path'],cnf,arm,proof,provenance],32,out)
            records=[json.loads(line) for line in stdout.read_text().splitlines() if line.startswith('{')]
            terminal=next((r for r in reversed(records) if r.get('record')=='terminal'),None)
            startup=[r for r in records if r.get('record') in ('derived-profile','common-startup','common-startup-fingerprint','selected-root-values')]
            if common is None:common=startup
            matched=bool(len(startup)==4 and startup==common)
            verdict={10:'SAT',20:'UNSAT'}.get(terminal.get('verdict') if terminal else None,'UNKNOWN')
            valid=False;stages=[];model=None
            if proof.exists():valid,stages=prefix(cnf,proof,scratch,manifest['checker_pins'],verdict)
            if verdict=='SAT':model=check_files(cnf,stdout)
            observed_use=bool(terminal and terminal.get('published')==3 and terminal.get('native_unit_uses',0)+terminal.get('native_conflict_uses',0)>0)
            certificate_censored=any(s.get('censored') for s in stages)
            if process.get('censored') or process.get('exit') in (124,137,-9,-15):classification='mechanism-censored'
            elif certificate_censored:classification='checker-censored-unaccepted'
            elif not valid or (verdict=='SAT' and not model[0]):classification='certificate-or-original-model-rejected'
            elif not terminal or terminal.get('failure_stage') in ('input','configuration'):classification='source-or-harness-failure'
            elif not matched:classification='common-startup-mismatch'
            elif arm=='on' and (process.get('exit')!=0 or not observed_use):classification='mechanism-inconclusive'
            elif arm=='off' and (process.get('exit')!=0 or terminal.get('published')!=0 or terminal.get('native_unit_uses',0) or terminal.get('native_conflict_uses',0)):classification='off-arm-harness-failure'
            else:classification='checked-native-consumption' if arm=='on' else 'checked-matched-off-prefix'
            result={'arm':arm,'process':process,'terminal':terminal,'records':records,'common_startup_matches':matched,'verdict':verdict,'original_cnf_sha256':cnf_sha,'proof_prefix_checked':valid,'checker_stages':stages,'original_model_check':model,'classification':classification,'native_consumption':observed_use,'solved':bool(process.get('exit')==0 and verdict in ('SAT','UNSAT') and valid and (verdict!='SAT' or model[0]))}
            results.append(result)
            for label,path in [('stdout',stdout),('native.dsr',proof),('provenance.jsonl',provenance)]:
                if path.exists():
                    with path.open('rb') as source,gzip.open(destination/f'{arm}.{label}.gz','wb') as out:
                        while chunk:=source.read(1048576):out.write(chunk)
            (destination/f'{arm}.json').write_text(json.dumps(result,indent=2)+'\n')
        integrated=[r['classification'] for r in results]==['checked-matched-off-prefix','checked-native-consumption']
        summary={'schema':'rank1161-public-guarded-cover-consumption-v1','manifest_sha256':sha(HERE/'manifest.json'),'source_commit':manifest['source_commit'],'driver_sha256':manifest['driver_pin']['sha256'],'controls':controls,'arms':results,'mechanism_integrated':integrated,'formal_benchmark_claim':False,'target_recovery_claim':False,'interpretation':'Only exact native publication plus actual native clause reason and independently accepted full original-CNF proof prefix counts as mechanism consumption; all other contexts are retained inconclusive or failed.'}
        (destination/'result.json').write_text(json.dumps(summary,indent=2)+'\n')
    except Exception as error:
        (destination/'failure.json').write_text(json.dumps({'failure':str(error),'arms':results,'controls':controls,'mechanism_integrated':False,'solved':False},indent=2)+'\n');raise
    finally:scratch.close()
if __name__=='__main__':main()
