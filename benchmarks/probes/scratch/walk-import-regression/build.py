from pathlib import Path
import subprocess,hashlib,json,shlex
root=Path(__file__).resolve().parents[4];work=Path(__file__).resolve().parent
base=root/'build/pin-parity-public-release-6fe14539-01'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
meta=json.loads((base/'PIN.json').read_text())
for name,h in meta['source_sha256'].items():assert sha(root/name)==h,name
source=(root/'src/policies/ops/walk/walk.cpp').read_text()
anchor='  bool imported = false;'
assert source.count(anchor)==1
source=source.replace(anchor,'''  // Diagnostic only: emulate the legacy saved-to-target copy privately.
#if DIAG_SAVED_GAPS
  for (Var v=0;v<k.nvars;++v) {
    if (k.value(make_lit(v,false))) continue;
    const int8_t phase=k.h->saved[v]<0?-1:1;
    w.values[make_lit(v,false)]=phase;
    w.values[make_lit(v,true)]=-phase;
    w.best_values[v]=phase;
  }
#endif
'''+anchor)
anchor='    local_search(w, steps);'
assert source.count(anchor)==1
source=source.replace(anchor,'''    const uint64_t preparation=w.steps;
    const uint32_t candidate_unsat=w.initial;
    local_search(w, steps + ((DIAG_KEEP_FLIPS && hint) ? preparation : 0));
    std::fprintf(stderr,"walk-import-diagnostic walk=%llu candidate=%llu ordinary=%u initial=%u minimum=%u prep=%llu steps=%llu flips=%llu saved_gaps=%d keep_flips=%d\\n",
      (unsigned long long)k.stats.walks,(unsigned long long)k.walk_seed_candidate,
      export_threshold,candidate_unsat,w.minimum,(unsigned long long)preparation,
      (unsigned long long)w.steps,(unsigned long long)w.flips,DIAG_SAVED_GAPS,DIAG_KEEP_FLIPS);''')
(work/'walk-diagnostic.cpp').write_text(source)
command_lines=subprocess.check_output(['ninja','-C',str(base),'-t','commands','tactix'],text=True).splitlines()
compile_line=next(x for x in command_lines if ' -c ' in x and x.endswith('/src/policies/ops/walk/walk.cpp'))
compile_base=shlex.split(compile_line)
link_line=command_lines[-1];assert link_line.startswith(': && ') and link_line.endswith(' && :')
link_base=shlex.split(link_line[5:-5]);walk_obj=next(x for x in link_base if x.endswith('/src/policies/ops/walk/walk.cpp.o'))
results=[]
for saved,keep in [(0,0),(0,1),(1,0),(1,1)]:
 name=f'saved{saved}-keep{keep}'
 pin=root/('build/pin-walk-import-e5f5d656-'+name);assert not pin.exists();pin.mkdir()
 (pin/'walk.cpp').write_text(source)
 cc=list(compile_base);cc[cc.index('-o')+1]=str(pin/'walk.o');cc[cc.index('-c')+1]=str(pin/'walk.cpp')
 # Remove generated dependency-file flags inherited from the reference object.
 for flag in ('-MF','-MT'):
  if flag in cc:
   at=cc.index(flag);del cc[at:at+2]
 if '-MD' in cc:cc.remove('-MD')
 cc+=['-DDIAG_SAVED_GAPS='+str(saved),'-DDIAG_KEEP_FLIPS='+str(keep)]
 subprocess.run(cc,cwd=base,check=True)
 link=list(link_base);link[link.index(walk_obj)]=str(pin/'walk.o');link[link.index('-o')+1]=str(pin/'tactix')
 inputs={str((base/x).relative_to(root)):sha(base/x) for x in link if x.endswith(('.o','.a')) and not Path(x).is_absolute()}
 subprocess.run(link,cwd=base,check=True)
 bindings={str(x.relative_to(root)):sha(x) for x in [pin/'walk.cpp',pin/'walk.o',pin/'tactix',base/'PIN.json',Path(__file__)]}
 (pin/'manifest.json').write_text(json.dumps(dict(scope='Diagnostic factorial only; no production adoption',compile=cc,link=link,link_cwd=str(base),source_sha256=meta['source_sha256'],link_inputs=inputs,bindings=bindings),indent=2)+'\n')
 results.append(dict(name=name,pin=str(pin.relative_to(root)),binary_sha256=sha(pin/'tactix'),saved_gaps=saved,keep_flips=keep))
for name,h in meta['source_sha256'].items():assert sha(root/name)==h,name
(work/'builds.json').write_text(json.dumps(results,indent=2)+'\n');print(results)
