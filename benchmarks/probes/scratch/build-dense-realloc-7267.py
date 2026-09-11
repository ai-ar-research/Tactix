#!/usr/bin/env python3
"""Freeze full dense-payload cutover and actual-parent capacity/state replay."""
from pathlib import Path
base = Path(__file__).with_name('build-occurrence-cutover-7253.py')
code = base.read_text()
changes = {
    "source = root / '.claude/worktrees/runtime-single-occurrence-cutover-7253'":
        "source = root / '.claude/worktrees/runtime-dense-realloc-7267'",
    "parent = root / '.claude/worktrees/runtime-single-occurrence-7249'":
        "parent = root / '.claude/worktrees/runtime-single-occurrence-cutover-7253'",
    '20260911-7253-occurrence-cutover-build':'20260911-7267-dense-realloc-build',
    'build/pin-7253-occurrence-cutover-':'build/pin-7267-dense-realloc-',
    'build/pin-7249-single-occurrence-':'build/pin-7253-occurrence-cutover-',
    "for fixture in ('test_connected_occurrence', 'test_occurrence_cutover',":
        "for fixture in ('test_realloc_words', 'test_flat_payload_replay', 'test_connected_occurrence', 'test_occurrence_cutover',",
    'One mixed owning payload plus exact large counts replaces Eliminate duplicate large payload. Probe remains contiguous and forward partial index remains distinct. No scheduling, budget, rule eligibility or proof change intended.':
        'Replace only FlatLists four-byte payload allocation with a realloc-backed three-pointer owner. Preserve vector capacities/values and all list offsets, growth, packing, logical order and physical counters; fresh-buffer defrag remains unchanged. Current7253 is parent; no budget/guard/proof-rule change. Realloc may still move/copy or fail and is not a promised RSS improvement.',
    'Exact old large-vs-mixed order asserted by same actual-parent/candidate fixture:52 cases364 boundaries across seven rule families, delayed binaries, duplicates, stale entries, in-place replacement, collection and mode reversal.840 replacement cases also exact. Storage adds retained-capacity rebuild and moved-owner controls. Full three suites and focused fixtures.':
        'New35815-operation vector value/size/capacity replay, forced allocation movement and null-failure preservation, overflow, tagged words, copy/move/reuse and two intentional fatal controls. New FlatLists physical capacity/offset/relocation/defrag oracle compiled against actual old/new headers. Existing52-case364-boundary actual-parent library oracle and840 replacement cases remain exact. Full three suites and focused storage/kernel/rule fixtures.',
    'Filtered traversal scans binary entries; whole-Eliminate/runtime/RSS repetitions decide net value. Physical relocation and memory diagnostics legitimately change. Fixed tests do not establish unrestricted equivalence, independent proof acceptance or fuzz coverage.':
        'The C allocation owner must preserve initialization, ownership, pointer invalidation and checked allocation failure. Growth may remain copying under the allocator; copying defrag is deliberately not claimed fixed. Current7265 ownership and later repeated runtime/RSS decide whether this addresses the remaining peak. No intrinsic speedup, global memory parity or fuzz acceptance inferred.',
}
for before,after in changes.items():
    assert before in code,before
    code = code.replace(before,after)
start = code.index('expected = {')
end = code.index('\nassert changed == expected',start)
code = code[:start] + "expected = {'src/kernel/realloc_words.hpp', 'src/kernel/occ.hpp', 'tests/test_realloc_words.cpp', 'tests/test_flat_payload_replay.cpp'}" + code[end:]
marker = '\nfor rel, digest in files.items(): assert sha(source / rel) == digest, rel\n'
extra = '''
for kind in ('release','bc','trace'):
    for flag in ('--fail-reserve','--overflow-resize'):
        command = [str(root / ('build/pin-7267-dense-realloc-' + kind) / 'tests/test_realloc_words'), flag]
        p = subprocess.run(command,cwd=c,capture_output=True,text=True)
        assert p.returncode == -6,(command,p.returncode,p.stdout,p.stderr)
        label = kind + flag + '-expected-abort'
        with (c/'logs'/(label+'.log')).open('x') as out: out.write(p.stdout+p.stderr)
        commands[label] = dict(command=command,exit_code=p.returncode,expected_signal='SIGABRT')
flat_parent = c/'parent-flat-fixture'
run('parent-flat-build',['/cm/local/apps/gcc/13.1.0/bin/g++','-std=c++20','-O3','-DNDEBUG',
    '-fno-exceptions','-fno-rtti','-I'+str(parent/'src'),
    source/'tests/test_flat_payload_replay.cpp','-o',flat_parent])
flat_oracle = run('parent-flat',[flat_parent])
assert flat_oracle == (c/'logs/release-test_flat_payload_replay.log').read_text()
'''
assert code.count(marker) == 1
code = code.replace(marker,extra+marker)
code = code.replace('oracle=oracle, replacement=replacement,','oracle=oracle, replacement=replacement, flat_oracle=flat_oracle,')
exec(compile(code,str(base),'exec'),{'__file__':str(Path(__file__)),'__name__':'__main__'})
