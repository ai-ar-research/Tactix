from pathlib import Path
import subprocess,json,hashlib
root=Path('/home/hwu/SAT-solving');folder=Path(__file__).resolve().parent
checkers={n:root/'tools/checkers/bin'/n for n in ['dsr-trim','srcheck','drat-trim']};model=root/'tools/fuzz/checkmodel'
results=[]
def run(args,log):
 p=subprocess.run([str(x) for x in args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT);log.write_bytes(p.stdout);return p.returncode,p.stdout
for cnf in sorted((folder/'artifacts').glob('*.cnf')):
 base=cnf.with_suffix('');format=int(base.name.rsplit('-',1)[1]);proof=base.with_suffix('.proof');lease=base.name.startswith('lease-')
 if not lease:
  code,output=run([model,cnf,base.with_suffix('.model')],base.with_suffix('.model-check.log'));assert code==0,(cnf,output)
 if format in [0,1]:
  flags=['-f','-c']+(['-U'] if lease else [])
  code,output=run([checkers['dsr-trim'],*flags,cnf,proof,base.with_suffix('.lsr')],base.with_suffix('.trim.log'))
  assert code==0 and b's VALID\n' in output,(cnf,code,output)
  code,output=run([checkers['srcheck'],cnf,base.with_suffix('.lsr'),'c'],base.with_suffix('.trestle.log'))
  assert code==0 and b's VALID\n' in output,(cnf,code,output)
 else:
  code,output=run([checkers['drat-trim'],cnf,proof,'-f'],base.with_suffix('.drat.log'))
  assert code==1 and b's DERIVATION' in output and b'VERIFIED derivation: all lemmas preserve satisfiability' in output,(cnf,code,output)
 bad=base.with_suffix('.bad.dsr');bad.write_text('-1 0\n')
 code,output=run([checkers['dsr-trim'],'-f','-c','-U',cnf,bad,base.with_suffix('.bad.lsr')],base.with_suffix('.bad.log'))
 assert code!=0 and any(x in output for x in [b'NOT VERIFIED',b'INVALID',b'is not RUP',b'ERROR']), (cnf,code,output)
 results.append({'cnf':str(cnf),'format':format,'lease_rup_only':lease,'full_prefix_independent':True,'unsupported_unit_rejected':True})
assert len(results)==20,len(results)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
result={'results':results,'checker_binaries':{n:{'path':str(p),'sha256':sha(p)} for n,p in checkers.items()},'model_checker_sha256':sha(model),'artifact_hashes':{str(p.relative_to(folder)):sha(p) for p in (folder/'artifacts').rglob('*') if p.is_file()}}
(folder/'prefix-check-results.json').write_text(json.dumps(result,indent=2)+'\n')
print('PASS 4 witnessless RUP and 16 lifecycle full nonrefuting prefixes;16 original models;20 unsupported-unit controls')
