import collections
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = next(p for p in Path(__file__).resolve().parents if (p / 'CLAUDE.md').exists())
output = root / (sys.argv[1] if len(sys.argv) > 1 else 'build/native-support-operation-reconciliation.json')
old = json.loads((root / 'benchmarks/reports/2026-10-02-shared-policy-operation-map/inventory.json').read_text())
rows = [
    ('Solve entry and session ownership', ['src/strategy/execution_scope.hpp:ArenaOrderingScope', 'src/strategy/heuristic_execution.hpp:HeuristicAttachmentRequest', 'src/strategy/policy_completion.hpp:PolicyCompletion', 'src/kernel/kernel.hpp:KernelStopPublisher']),
    ('Entry trail normalization', ['src/strategy/action_execution.hpp:RestartPurpose']),
    ('Startup certificate selection', ['src/strategy/action_execution.hpp:CloseStartupCertificateRequest', 'src/strategy/lucky_execution.hpp:LuckyRequest']),
    ('Structural startup model/proof actions', ['src/strategy/lucky_execution.hpp:LuckyOpportunity', 'src/strategy/parameter_execution.hpp:ParameterInstallationRequest']),
    ('Early/late ordinary Lucky', ['src/strategy/lucky_execution.hpp:LuckyOpportunity']),
    ('Structured root program', ['src/strategy/online_sequence.hpp:OnlineStrategySequence', 'src/strategy/online_inprocess.cpp:OnlineStrategyExecutor']),
    ('Startup BCE cleanup', ['src/policies/default_policy.cpp:post-structured-amo', 'src/rules/bce/bce.cpp:bce_apply', 'src/strategy/publication_execution.hpp:RuleApplicationRequest']),
    ('Cold-state/residual guidance setup', ['src/strategy/representation_execution.hpp:MaterializeColdStateRequest', 'src/strategy/heuristic_execution.hpp:ResidualActivationRequest', 'src/strategy/heuristic_execution.hpp:ResidualImportRequest']),
    ('Sparse root collection', ['src/strategy/representation_execution.hpp:SparseCollectRequest']),
    ('Propagate', ['src/rules/propagate/propagate.cpp:propagate_apply', 'src/strategy/publication_execution.hpp:RuleApplicationRequest']),
    ('ConflictAnalysis', ['src/strategy/conflict_analysis_execution.hpp:NativeConflictAnalysisRequest']),
    ('Reduce', ['src/strategy/reduction_execution.hpp:ReductionPreparationRequest', 'src/strategy/reduction_execution.hpp:ReductionRequest']),
    ('Mode/configuration', ['src/strategy/guidance_execution.hpp:SwitchModeRequest', 'src/policies/restart_controller.hpp:resume_search']),
    ('Scheduled Restart', ['src/strategy/action_execution.hpp:RestartRequest', 'src/policies/restart_controller.hpp:PartialContinuation']),
    ('Root preparation backtrack', ['src/strategy/root_execution.hpp:RootPreparationRequest']),
    ('Reorder', ['src/strategy/guidance_execution.hpp:ReorderRequest']),
    ('Rephase', ['src/strategy/guidance_execution.hpp:RephaseRequest', 'src/strategy/root_execution.hpp:RootPreparationRequest']),
    ('Warm-up', ['src/strategy/guidance_execution.hpp:WarmupRequest']),
    ('Walk and local search guidance', ['src/strategy/guidance_execution.hpp:WalkRequest', 'src/strategy/guidance_execution.hpp:LocalSearchRotationRequest']),
    ('Probe/Eliminate entry/exit', ['src/strategy/execution_scope.hpp:ExecutionScope', 'src/strategy/occurrence_execution.hpp:OccurrenceSession', 'src/policies/default_policy.cpp:InprocessPhaseExit']),
    ('Dosed rule instances', ['src/strategy/bve_invocation.hpp:BveInvocationSession', 'src/strategy/parameter_execution.hpp:BeginBveDefinitionSessionRequest', 'src/strategy/publication_execution.hpp:RuleApplicationRequest']),
    ('Vivify', ['src/strategy/vivify_execution.hpp:VivifyBandRequest', 'src/strategy/vivify_execution.hpp:VivifyDoseScope']),
    ('Sweep', ['src/strategy/sweep_execution.hpp:SweepRequest']),
    ('Substitute', ['src/strategy/substitute_execution.hpp:SubstituteBatchRequest']),
    ('Strengthen/forward closure', ['src/strategy/work_execution.hpp:WorkSettingsScope', 'src/policies/default_policy.cpp:ForwardClosure', 'src/strategy/root_execution.hpp:SparseResumeRequest']),
    ('Dense/garbage/variable support', ['src/strategy/representation_execution.hpp:CollectGarbageRequest', 'src/strategy/representation_execution.hpp:CompactVariablesRequest', 'src/strategy/parameter_execution.hpp:InstallMaterialSettings']),
    ('Decide', ['src/strategy/action_execution.hpp:DecisionRequest']),
    ('Termination/interruption', ['src/strategy/policy_completion.hpp:finish', 'src/strategy/heuristic_execution.hpp:HeuristicDetachmentRequest', 'src/strategy/policy_feedback.hpp:finish']),
]
assert len(rows) == 28
bindings = []
for group, anchors in rows:
    found = []
    for anchor in anchors:
        file, needle = anchor.split(':', 1)
        hits = [i + 1 for i, line in enumerate((root / file).read_text().splitlines()) if needle in line]
        assert hits, (group, anchor)
        found.append({'file': file, 'anchor': needle, 'lines': hits})
    bindings.append({'operation_group': group, 'source_bindings': found})
search_paths = list((root / 'src/strategy').glob('*execution*')) + [
    root / 'src/policies/default_policy.cpp', root / 'src/policies/random_controller_policy.cpp',
    root / 'src/policies/restart_controller.hpp']
lexical = []
for entry in old['execution_and_support_sites']:
    hits = []
    for path in search_paths:
        if not path.is_file():
            continue
        for i, line in enumerate(path.read_text().splitlines(), 1):
            if re.search(r'\b' + re.escape(entry['symbol']) + r'\s*\(', line):
                hits.append({'file': str(path.relative_to(root)), 'line': i, 'text': line.strip()})
    lexical.append({'original': entry, 'current_occurrences': hits,
                    'scope': 'Lexical locator only: definitions, predicates and atomic primitives are not separate scheduled actions.'})
files = {b['file'] for row in bindings for b in row['source_bindings']}
files.update(e['file'] for e in old['execution_and_support_sites'])
files.update(str(p.relative_to(root)) for p in search_paths if p.is_file())
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
data = {
    'schema': 'tactix-shared-policy-reconciliation-v1',
    'parent_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'accepted_checkpoint': old['solver_source_checkpoint'],
    'scope': '28 manually reconciled operation groups with source-binding locators and all 49 original lexical entries; no runtime-reach or whole-migration acceptance claim.',
    'groups': bindings, 'original_lexical_sites': lexical,
    'source_sha256': {f: sha(root / f) for f in sorted(files)},
    'kernel_diff_from_accepted_checkpoint': subprocess.check_output(['git', 'diff', '12e3c4f0', '--', 'src/kernel'], cwd=root, text=True),
}
coverage = {}
for profile in ('debug', 'diagnostic'):
    build = root / ('build/shared-policy-prep-' + profile)
    paths = sorted((build / 'native-support-correspondence/enabled').glob('*.feedback.jsonl'))
    paths += sorted((build / 'native-support-signature/enabled').glob('*.feedback.jsonl'))
    calls, progress, supports = collections.Counter(), collections.Counter(), collections.Counter()
    intervals = 0
    for path in paths:
        for line in path.read_text().splitlines():
            row = json.loads(line)
            if row['type'] == 'interval':
                intervals += 1
            if row['type'] != 'action':
                continue
            calls[row['identity']] += 1
            if row['outcome']['progress']:
                progress[row['identity']] += 1
            if row['kind'] == 3:
                supports[row['identity']] += 1
                assert not row['outcome']['applications'] and not row['outcome']['progress']
                if row['outcome']['unit'] == 0:
                    assert not row['outcome']['work'] and not row['outcome']['composition_overshoot']
    coverage[profile] = {'streams': len(paths), 'actions': sum(calls.values()),
                         'support_receipts': sum(supports.values()), 'intervals': intervals,
                         'calls': dict(sorted(calls.items())), 'outcome_progress_marks': dict(sorted(progress.items())),
                         'scope': 'Observed fixture receipts, not comprehensive reach. A progress mark is not automatically a durable formula change.'}
data['fixture_coverage'] = coverage
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(data, indent=2) + '\n')
print('28 bound groups; 49 retained original lexical entries; kernel diff bytes', len(data['kernel_diff_from_accepted_checkpoint']))
