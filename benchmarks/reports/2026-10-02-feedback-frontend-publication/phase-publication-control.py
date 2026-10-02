import hashlib,json,pathlib,resource,shlex,signal,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
build=root/sys.argv[1]
args=shlex.split((build/'tests/CMakeFiles/test_native_policy_feedback.dir/link.txt').read_text())
args[args.index('-o')+1]='feedback_phase_publication_before'
needle='../CMakeFiles/tactix_lib.dir/src/policies/default_policy.cpp.o'
args[args.index(needle)]=str(build/'frontend-feedback-before-default.o')
subprocess.run(args,cwd=build/'tests',check=True)
def no_core(): resource.setrlimit(resource.RLIMIT_CORE,(0,0))
result=subprocess.run([str(build/'tests/feedback_phase_publication_before')],capture_output=True,text=True,timeout=30,preexec_fn=no_core)
assert result.returncode==-signal.SIGABRT,result
assert '!k.factor_selection && !k.dense && !k.inprocessing' in result.stderr,result.stderr
print('Preserved a4cfabe3 slot delivery rejected by restored phase-lifetime assertion.')
(build/'feedback-phase-publication-control.json').write_text(json.dumps({
'preserved_commit':'a4cfabe3','adaptations':'None to preserved DefaultPolicy source/headers; current fixture and common machinery shared.',
'baseline_source_sha256':hashlib.sha256((build/'frontend-feedback-before-default.cpp').read_bytes()).hexdigest(),
'fixture_source_sha256':hashlib.sha256((root/'tests/test_native_policy_feedback.cpp').read_bytes()).hexdigest(),
'binary_sha256':hashlib.sha256((build/'tests/feedback_phase_publication_before').read_bytes()).hexdigest(),
'returncode':result.returncode,'stderr':result.stderr,'expected_rejection':True},indent=2)+'\n')
