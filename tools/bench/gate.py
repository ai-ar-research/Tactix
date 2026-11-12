#!/usr/bin/env python3
"""Fixed-set regression gates: prepare candidate-only campaigns and score them.

No gate reruns a reference solver. Three frozen authorities exist:

- ``fixed-*``: the retained Rank 1-50 Kissat fixed-reference vector under the
  12-CPU/32-GiB/1,200-second contract, measured through whole-run PBP.
- ``controller-*``: the paper-target bounded regression gates (B28+C5,
  Rank 1-50, WV100). The preserved paper release is the per-instance authority;
  aggregate solver time may be at most ten percent slower, and censored paper
  outcomes remain explicit exceptions rather than independent verification.
- ``tierd-*``: full Tier D against the paper's 32 GiB campaign and its 64 GiB
  checker recheck. A candidate must retain every paper solve and verification
  and stay within ten percent of both raw-solver and verified PAR-2.
"""
import argparse
from collections import Counter
import csv
import datetime as dt
import hashlib
import json
import math
from pathlib import Path
import shlex
import shutil
import subprocess

from experimental_tierd import check_failure_class
from proof_oracle import pin_dsr_transport
from runtime_task import sha256 as sha

ROOT = Path(__file__).resolve().parents[2]
CANONICAL_CHECKERS = {
    'dsr-trim': '3b5c773706dc4695f08115ce862e991bfa2e44e3388df80f40ae8b0823dfbdee',
    'dsr-compress': 'a44102ce11162a4687450a4c3a640d27a49703072dd29802bd259676eb668d55',
    'srcheck': '19bf79f49e293bb0df82b1e4ec82ec45987c2791864598bada12525ef41a2945',
    'drat-trim': 'f2cb7d45d6acc05b1295bfa0d27b53d75fa48259c2b1e15f15772f064f99849a',
    'cake_lpr': 'ab9dc6c43ad0f55bbaf336c5d898919ddb03d0bdb75e88dff48237fb06ba1f57',
    'veripb': '2e5fa5ef07c655640e6fff0d3f41561eab1502ec2e48766cca60287625935674',
    'cake-pb-cnf': 'c859a9c926ca8c63dac3317930ea8dbe3732a6fc24faa6e28ed740fadd67dc47',
}
RUNNER_PINS = ('runtime_task.py', 'proof_oracle.py', 'anonymous_scratch.py')
SOLVER_EXIT = {'SAT': 10, 'UNSAT': 20}
# Each runtime proof pipeline ends in exactly one independent verifier.
FINAL_VERIFIER = {'dsr-as-drat-drat-trim-cake_lpr': 'cake_lpr',
                  'dsr-trim-dsr-compress-trestle': 'srcheck',
                  'veripb-cake-pb': 'cake-pb-cnf'}

REFERENCE = ROOT / 'benchmarks/baselines/rank1-50-kissat-12cpu-v1/manifest.json'
FIXED_SCHEMAS = tuple(f'tactix-fixed-reference-runtime-v{n}' for n in range(2, 8))
MODEL_CHECKER_PINS = ROOT / 'benchmarks/baselines/sc24-26-32g-5000s/pins'

PAPER_COMMIT = '0f7c10174faa13e41e3ab60654bf8fc3322f4041'
PAPER_BINARY = 'a7c66babcd568a3497e0b46271b0d25941c59042333b252527f63e40247a4907'
RUNTIME_FACTOR = 1.10
CONTROLLER_BASELINE = ROOT / 'benchmarks/baselines/controller-paper-v1/results.json'
CONTROLLER_SETS = {
    'b28c5': ('20261015-prod-samewindow-regression-b28c5', 33),
    'rank1-50': ('20261015-prod-samewindow-regression-rank1-50', 50),
    'wv100': ('20261015-prod-samewindow-regression-wv100', 100),
}
CHECK_TIMEOUT = 1800
TIERD_BASE = ROOT / 'benchmarks/runs/20261010-tierd-fastel'
TIERD_RECHECK = ROOT / 'benchmarks/runs/20261011-tierd-fastel-recheck-64g'
TIERD_BASELINE = ROOT / 'benchmarks/baselines/tierd-paper-v1/results.json'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def finite_seconds(value):
    return isinstance(value, (int, float)) and math.isfinite(value) and value >= 0


def start_campaign(campaign, dsr_transport):
    """Create a fresh runtime_task campaign with the shared runner pins."""
    campaign.mkdir()
    for name in ('pins', 'logs', 'results', 'provisional', 'claims'):
        (campaign / name).mkdir()
    for name in RUNNER_PINS:
        shutil.copy2(ROOT / 'tools/bench' / name, campaign / 'pins' / name)
    pin_dsr_transport(dsr_transport, campaign / 'pins')


def array_batch(campaign, *, shebang, job, partition, cpus, memory_mib, time,
                array, environment=''):
    return f'''{shebang}
#SBATCH --job-name={job}
#SBATCH --partition={partition}
#SBATCH --ntasks=1
#SBATCH --cpus-per-task={cpus}
#SBATCH --mem={memory_mib}M
#SBATCH --time={time}
#SBATCH --array={array}
#SBATCH --no-requeue
#SBATCH --output={campaign}/logs/%A_%a.log
set -euo pipefail
ulimit -c 0
{environment}exec python3 {shlex.quote(str(campaign / 'pins/runtime_task.py'))} {shlex.quote(str(campaign))} "$SLURM_ARRAY_TASK_ID"
'''


def pin_hashes(campaign):
    return {str(p.relative_to(campaign)): sha(p)
            for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv',
                      campaign / 'array.sbatch']}


def verify_pins(campaign, manifest, label='pin changed: '):
    for relative, digest in manifest['pin_sha256'].items():
        require(sha(campaign / relative) == digest, label + relative)


def read_matrix(campaign):
    with (campaign / 'matrix.tsv').open(newline='') as source:
        return list(csv.DictReader(source, delimiter='\t'))


def write_score(path, result, replace=False):
    if path.exists():
        require(replace, 'score output already exists')
        prior = json.loads(path.read_text())
        require(prior.get('schema') == result['schema'] and
                prior.get('manifest_sha256') == result['manifest_sha256'] and
                prior.get('target_baseline_sha256') == result['target_baseline_sha256'],
                'refusing to replace score for a different campaign or target')
    temporary = path.with_name('.' + path.name + '.tmp')
    require(not temporary.exists(), 'temporary score output already exists')
    temporary.write_text(json.dumps(result, indent=2) + '\n')
    temporary.replace(path)


# Fixed Rank 1-50 reference ------------------------------------------------

def reference(path=REFERENCE):
    ref = json.loads(path.read_text())
    sizes = {'tactix-fixed-rank1-40-kissat-reference-v1': 40,
             'tactix-fixed-rank1-40-reference-v1': 40,
             'tactix-fixed-rank1-50-kissat-reference-v1': 50,
             'tactix-fixed-rank1-50-reference-v1': 50,
             'tactix-fixed-b27c5-reference-v1': 32,
             'tactix-fixed-b28c5-reference-v1': 33}
    require(ref['schema'] in sizes, 'reference schema')
    count = sizes[ref['schema']]
    rows = ref['records']
    require(len(rows) == count and {r['rank'] for r in rows} == set(range(1,count+1)), 'reference rank coverage')
    require(len({r['instance'] for r in rows}) == count, 'duplicate reference input')
    if 'original_reference' in ref:
        binding = ref['original_reference']
        original_path = ROOT / binding['path']
        require(sha(original_path) == binding['sha256'], 'original reference changed')
        original = reference(original_path)
        require(rows[:40] == original['records'], 'original forty measurements changed')
        for key in ('partition','cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
            require(ref['resource_contract'][key] == original['resource_contract'][key], 'extended resource mismatch: ' + key)
    total = math.fsum(r['measurement']['solve']['wall_seconds'] for r in rows)
    require(math.isclose(total, ref['solver_seconds'], rel_tol=0, abs_tol=1e-8), 'reference total')
    return ref


def candidate_rows(ref, arms=('tactix-pbp', 'tactix-drat')):
    rows = []
    for r in sorted(ref['records'], key=lambda r:r['rank']):
        for arm in arms:
            rows.append(dict(task=len(rows), rank=r['rank'], arm=arm,
                             tier=r.get('tier', 'D'),
                             instance=r['instance'], expected=r['expected']))
    return rows


def reference_identity(ref):
    return [(row['rank'], row['instance'], row['input_sha256'], row['expected'])
            for row in ref['records']]


def fixed_prepare(campaign, authority, release, bc, reference_path=REFERENCE,
                  runtime_target_seconds=None,
                  checker_pins=None, dsr_transport=None):
    campaign = campaign.resolve()
    require(campaign.parent == ROOT / 'benchmarks/runs', 'campaign must be directly under benchmarks/runs')
    require(not campaign.exists(), 'campaign already exists')
    ref = reference(reference_path)
    a = json.loads((authority / 'manifest.json').read_text())
    for rel,digest in a['files'].items():
        require(sha(ROOT / a['source'] / rel) == digest, 'candidate source changed: ' + rel)
    for binary in (release, bc):
        require(sha(binary) == a['binaries'][str(binary.resolve().relative_to(ROOT))], 'candidate pin changed')
    require(sha(release) == sha(bc), 'independent Release binaries differ')
    require(checker_pins is not None, 'explicit canonical --checker-pins required')
    checker_pins = checker_pins.resolve()
    require(checker_pins.is_relative_to(ROOT), 'checker pins must be in repository')
    for name, digest in CANONICAL_CHECKERS.items():
        path = checker_pins / name
        require(path.is_file() and sha(path) == digest,
                'canonical checker missing or changed: ' + name)
    for r in ref['records']:
        require(sha(ROOT / 'benchmarks/cache' / r['instance']) == r['input_sha256'], 'input changed')
    start_campaign(campaign, dsr_transport)
    for name in ('checkmodel', 'modelcheck.py'):
        shutil.copy2(MODEL_CHECKER_PINS / name, campaign / 'pins' / name)
    for name in CANONICAL_CHECKERS:
        shutil.copy2(checker_pins / name, campaign / 'pins' / name)
        require(sha(campaign / 'pins' / name) == CANONICAL_CHECKERS[name],
                'copied canonical checker changed: ' + name)
    shutil.copy2(release, campaign / 'pins/tactix')
    shutil.copy2(authority / 'cumulative.patch', campaign / 'pins/candidate-source.patch')
    selected_arms = ('tactix-pbp',)
    rows = candidate_rows(ref, selected_arms)
    with (campaign / 'matrix.tsv').open('x', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0], delimiter='\t', lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    array = f'0-{len(rows)-1}'
    (campaign / 'array.sbatch').write_text(array_batch(
        campaign, shebang='#!/usr/bin/env bash', job='tactix-pbp-gate',
        partition='cpu-dense-hwulab-q', cpus=12, memory_mib=32768,
        time='05:00:00', array=array))
    arm = dict(path='pins/tactix', sha256=sha(release), source_commit=a['production'],
        source_build=str(release.parent.relative_to(ROOT)), arguments=[],
        source_patch='pins/candidate-source.patch', source_patch_sha256=sha(authority / 'cumulative.patch'))
    # The command-line default is automatic selection: spell whole-run PBP.
    arms = {'tactix-pbp': dict(arm, arguments=['--pbp'], certificate='pbp')}
    proof_mode = 'whole-run PBP'
    contract = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                    memory_mib=32768, array=array, throttle=None,
                    node_constraint=None, proof_mode=proof_mode,
                    one_solver_per_task=True, solve_timeout=1200,
                    checker_timeout=16000)
    if runtime_target_seconds is not None:
        require(math.isfinite(runtime_target_seconds) and
                runtime_target_seconds > 0, 'runtime target')
    manifest = dict(schema='tactix-fixed-reference-runtime-v7', created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
        arms=arms, input_sha256={r['instance']:r['input_sha256'] for r in ref['records']},
        resource_contract=contract, collect_runtime_context=True,
        scratch_policy='anonymous-local-files-v1',
        scratch_contract='Linux O_TMPFILE under /tmp for CNF, proof, model and checker intermediates; kernel cleanup on exit, timeout, crash, OOM and cancellation.',
        candidate_authority=dict(path=str((authority / 'manifest.json').relative_to(ROOT)),sha256=sha(authority / 'manifest.json')),
        checker_authority=dict(path=str(checker_pins.relative_to(ROOT)),
                               sha256=CANONICAL_CHECKERS),
        fixed_reference=dict(path=str(reference_path.relative_to(ROOT)),sha256=sha(reference_path)),
        purpose=(f'{len(ref["records"])} whole-run-PBP solves against the retained reference vector; '
                 'no reference solver rerun.'),
        test_consideration=(a['test_consideration'] +
            ' Current runtime protocol checks routed PBP/DSR certificates, task identity and anonymous cleanup.'),
        validation=f'Whole-run PBP is elaborated by VeriPB and checked by CakePB; every stage must verify. Expected solves allow provisional development; all {len(rows)} candidate results, retained reference certificates, the companion B28+C5 gate, memory and Claude fuzz are required for acceptance.',
        limitation='Cross-window fixed reference; not isolated unit-cost attribution or promotion.')
    if runtime_target_seconds is not None:
        manifest['acceptance_runtime_seconds'] = runtime_target_seconds
    manifest['pin_sha256'] = pin_hashes(campaign)
    (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for p in [*(campaign / 'pins').iterdir(), campaign / 'matrix.tsv',
              campaign / 'array.sbatch', campaign / 'manifest.json']:
        p.chmod(p.stat().st_mode & ~0o222)
    return manifest


def verify_campaign(campaign):
    manifest_path = campaign / 'manifest.json'
    require(manifest_path.is_file(), 'prepare campaign first')
    manifest = json.loads(manifest_path.read_text())
    require(manifest.get('schema') in FIXED_SCHEMAS, 'campaign schema')
    binding = manifest['fixed_reference']
    require(sha(ROOT / binding['path']) == binding['sha256'],
            'fixed reference changed')
    verify_pins(campaign, manifest)
    reference(ROOT / binding['path'])
    return manifest


def submit(campaign):
    campaign = campaign.resolve()
    verify_campaign(campaign)
    launch = campaign / 'launch.json'
    require(not launch.exists(), 'campaign already submitted')
    output = subprocess.run(
        ['sbatch', '--parsable', str(campaign / 'array.sbatch')],
        check=True, capture_output=True, text=True).stdout.strip()
    job_id = output.split(';', 1)[0]
    with (campaign / 'matrix.tsv').open(newline='') as source:
        tasks = sum(1 for _ in csv.DictReader(source, delimiter='\t'))
    record = dict(job_id=job_id,
                  tasks=tasks,
                  manifest_sha256=sha(campaign / 'manifest.json'),
                  submitted_at=dt.datetime.now(dt.timezone.utc).isoformat())
    with launch.open('x') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(job_id)
    return record


def score_records(ref, records, arm='tactix', runtime_target_seconds=None,
                  require_canonical_dsr=False, require_automatic_routes=False,
                  require_pbp_route=False):
    expected = {r['rank']:r for r in ref['records']}
    seen = set()
    total = 0.0
    solved = checked = 0
    per_rank = []
    for r in records:
        rank = int(r['rank'])
        require(rank in expected and rank not in seen, 'unknown/duplicate candidate rank')
        seen.add(rank)
        base = expected[rank]
        require(r['arm'] == arm and r['instance'] == base['instance'] and r['expected'] == base['expected'], 'candidate identity mismatch')
        s = r['solve']
        require(math.isfinite(s['wall_seconds']) and s['wall_seconds'] >= 0, 'invalid candidate time')
        # A clean timeout with no verdict has no certificate to reject. Keep
        # it in PAR-2 after final cleanup, just as in the provisional view.
        clean_timeout = (s['timed_out'] and r.get('verdict') == 'UNKNOWN' and
                         r.get('proof_check') is None and r.get('model_check') is None)
        require('valid' not in r or r['valid'] or clean_timeout, 'rejected candidate certificate')
        if require_canonical_dsr and arm == 'tactix-dsr' and base['expected'] == 'UNSAT' and r.get('valid'):
            check = r.get('proof_check') or {}
            names = [stage.get('name') for stage in check.get('stages', [])]
            route = check.get('solver_reported_route')
            expected_names = ({('drat-trim', 'cake_lpr')}
                              if route == 'dsr-cake_lpr' else
                              {('dsr-trim', 'srcheck'),
                               ('dsr-trim', 'dsr-compress', 'srcheck')}
                              if route == 'dsr-trestle' else set())
            require(check.get('result') == 'verified' and
                    tuple(names) in expected_names and
                    all(stage.get('result') == 'verified' for stage in check['stages']),
                    'explicit DSR gate requires the hybrid checker pipeline')
        if require_automatic_routes and arm == 'tactix-auto' and base['expected'] == 'UNSAT' and r.get('valid'):
            check = r.get('proof_check') or {}
            routes = {
                'veripb-cake-pb': ['veripb', 'cake-pb-cnf'],
                'dsr-trim-dsr-compress-trestle': ['dsr-trim', 'dsr-compress', 'srcheck'],
                'dsr-as-drat-drat-trim-cake_lpr': ['drat-trim', 'cake_lpr'],
            }
            names = [stage.get('name') for stage in check.get('stages', [])]
            expected_route = {'veripb-cake-pb': 'pbp',
                              'dsr-trim-dsr-compress-trestle': 'dsr-trestle',
                              'dsr-as-drat-drat-trim-cake_lpr': 'dsr-cake_lpr'}.get(
                                  check.get('pipeline'))
            require(check.get('result') == 'verified' and
                    check.get('solver_reported_route') == expected_route and
                    names == routes.get(check.get('pipeline')) and
                    all(stage.get('result') == 'verified' for stage in check['stages']),
                    'automatic proof route differs from plan section 5')
        if require_pbp_route and arm == 'tactix-pbp' and base['expected'] == 'UNSAT' and r.get('valid'):
            check = r.get('proof_check') or {}
            require(check.get('result') == 'verified' and
                    check.get('solver_reported_route') == 'pbp' and
                    check.get('pipeline') == 'veripb-cake-pb' and
                    [stage.get('name') for stage in check.get('stages', [])] ==
                    ['veripb', 'cake-pb-cnf'] and
                    all(stage.get('result') == 'verified' for stage in check['stages']),
                    'whole-run PBP route differs from the release contract')
        ok = r['expected_match'] and not s['timed_out'] and s['exit_code'] == SOLVER_EXIT[base['expected']]
        solved += ok
        checked += bool(r.get('valid') and r.get('artifact_deleted') and r.get('derived_artifacts_deleted') and not r.get('cleanup_pending'))
        seconds = s['wall_seconds'] if ok else 2 * ref['resource_contract']['solve_timeout']
        total += seconds
        per_rank.append(dict(rank=rank, solved=ok, candidate_par2_seconds=seconds,
            reference_seconds=base['measurement']['solve']['wall_seconds'],
            candidate_peak_rss_kb=s.get('peak_rss_kb'),
            reference_peak_rss_kb=base['measurement']['solve'].get('peak_rss_kb')))
    complete = seen == set(expected)
    target = (1.01 * ref['solver_seconds'] if runtime_target_seconds is None
              else runtime_target_seconds)
    require(math.isfinite(target) and target > 0, 'runtime target')
    return dict(records=len(records), expected_solves=solved, checked_clean=checked,
        complete=complete, candidate_par2=total if complete else None,
        fixed_reference_seconds=ref['solver_seconds'],
        runtime_target_seconds=target,
        ratio=total / ref['solver_seconds'] if complete else None,
        runtime_target_met=bool(complete and solved == len(expected) and
                                total <= target),
        per_rank=sorted(per_rank,key=lambda r:r['rank']))


def fixed_score(campaign, comparison_reference=None, runtime_target_seconds=None):
    m = json.loads((campaign / 'manifest.json').read_text())
    binding = m['fixed_reference']
    require(sha(ROOT / binding['path']) == binding['sha256'], 'fixed reference changed')
    bound_ref = reference(ROOT / binding['path'])
    ref = bound_ref
    comparison_binding = None
    if comparison_reference is not None:
        comparison_reference = comparison_reference.resolve()
        ref = reference(comparison_reference)
        require(reference_identity(ref) == reference_identity(bound_ref),
                'comparison reference identity mismatch')
        comparison_binding = dict(
            path=str(comparison_reference.relative_to(ROOT)),
            sha256=sha(comparison_reference))
    schema = m.get('schema')
    if schema in FIXED_SCHEMAS:
        expected_arms = tuple(m['arms'])
        automatic = schema == 'tactix-fixed-reference-runtime-v6'
        pbp = schema == 'tactix-fixed-reference-runtime-v7'
        main_arm = 'tactix-pbp' if pbp else 'tactix-auto' if automatic else 'tactix-dsr'
        require(expected_arms in ((main_arm,), (main_arm, 'tactix-drat')),
                'arm mismatch')
        proof_mode = (('whole-run PBP' if pbp else
                       'automatic PBP/DSR' if automatic else 'native DSR') +
                      (' plus same-source DRAT' if len(expected_arms) == 2 else ''))
        required = dict(partition='cpu-dense-hwulab-q', cpus_per_task=12,
                        memory_mib=32768, solve_timeout=1200,
                        checker_timeout=16000, one_solver_per_task=True,
                        proof_mode=proof_mode)
        for key, value in required.items():
            require(m['resource_contract'].get(key) == value,
                    'resource mismatch: ' + key)
        if schema == 'tactix-fixed-reference-runtime-v5':
            require(m.get('require_canonical_dsr') is True,
                    'canonical DSR gate requirement missing')
        require(m.get('scratch_policy') == 'anonymous-local-files-v1',
                'scratch policy mismatch')
        # Whole-run PBP was flagless before the automatic default and is
        # spelled `--pbp` since.
        require(m['arms'][main_arm].get('certificate') == ('pbp' if pbp else 'auto' if automatic else 'dsr') and
                m['arms'][main_arm].get('arguments') in
                ((['--auto-proof'],) if automatic else ([], ['--pbp']) if pbp else ([],)),
                'main proof arm mismatch')
        if len(expected_arms) == 2:
            require(m['arms']['tactix-drat'].get('certificate') == 'drat' and
                    m['arms']['tactix-drat'].get('arguments') == ['--drat'],
                    'DRAT arm mismatch')
            require(m['arms'][main_arm]['sha256'] ==
                    m['arms']['tactix-drat']['sha256'], 'control binary mismatch')
    else:
        for key in ('cpus_per_task','memory_mib','solve_timeout','checker_timeout','one_solver_per_task'):
            require(m['resource_contract'][key] == ref['resource_contract'][key], 'resource mismatch: ' + key)
        require(m['resource_contract']['proof_mode'] == 'DRAT', 'proof mode mismatch')
        expected_arms = ('tactix',)
    require(m['input_sha256'] == {r['instance']:r['input_sha256'] for r in ref['records']}, 'input hashes mismatch')
    verify_pins(campaign, m)
    matrix = {int(row['task']): row for row in read_matrix(campaign)}
    require(len(matrix) == len(ref['records']) * len(expected_arms),
            'matrix coverage mismatch')
    records = {}
    for folder in ('provisional','results'):
        for p in (campaign / folder).glob('*.json'):
            r = json.loads(p.read_text())
            key = (r['arm'], int(r['rank']))
            task = int(r['task'])
            require(int(p.stem) == task and task in matrix,
                    'task identity mismatch')
            row = matrix[task]
            require(all(str(r[field]) == row[field]
                        for field in ('rank', 'arm', 'instance', 'expected')),
                    'matrix identity mismatch')
            if folder == 'results':
                require(r['binary_sha256'] == m['arms'][r['arm']]['sha256'], 'binary identity mismatch')
                if key in records:
                    for field in ('solve','certificate_sha256','certificate_bytes'):
                        require(records[key][field] == r[field], 'provisional/final mismatch')
            records[key] = r
    target = (m.get('acceptance_runtime_seconds')
              if runtime_target_seconds is None else runtime_target_seconds)
    arm_scores = {arm: score_records(
        ref, [record for (record_arm, _), record in records.items()
              if record_arm == arm], arm, target,
        require_canonical_dsr=(schema == 'tactix-fixed-reference-runtime-v5'),
        require_automatic_routes=(schema == 'tactix-fixed-reference-runtime-v6'),
        require_pbp_route=(schema == 'tactix-fixed-reference-runtime-v7'))
        for arm in expected_arms}
    output = arm_scores[expected_arms[0]]
    if len(expected_arms) == 2:
        main = arm_scores[expected_arms[0]]
        drat = arm_scores['tactix-drat']
        ratio_name = ('pbp_to_drat_ratio' if schema == 'tactix-fixed-reference-runtime-v7'
                      else 'auto_to_drat_ratio' if schema == 'tactix-fixed-reference-runtime-v6'
                      else 'dsr_to_drat_ratio')
        output = dict(main, arms=arm_scores,
                      **{ratio_name: (main['candidate_par2'] /
                                         drat['candidate_par2']
                                         if main['complete'] and drat['complete']
                                         and drat['candidate_par2'] else None)})
    reference_checked = int(ref.get('checked_at_freeze', 0))
    if ref.get('schema') == 'tactix-fixed-rank1-50-kissat-reference-v1':
        original = reference(ROOT / ref['original_reference']['path'])
        reference_checked = (int(original.get('checked_at_freeze', 0)) +
                             int(ref.get('extension_checked_at_freeze', 0)))
    for row in ref['records']:
        r = row['measurement']
        if 'source_result' in row:
            source = row['source_result']
            p = ROOT / source['path']
            if not p.exists():
                continue
            require(sha(p) == source['sha256'], 'reference result changed')
        else:
            p = ROOT / row.get('source_campaign', ref['source_campaign']) / 'results' / f'{int(r["task"]):03}.json'
        if not p.exists(): continue
        final = json.loads(p.read_text())
        require(final['valid'] and final['expected_match'], 'rejected reference certificate')
        for key in ('solve','certificate_sha256','certificate_bytes','instance','rank','arm'):
            require(final[key] == r[key], 'reference measurement changed')
        reference_checked += bool(final.get('artifact_deleted') and final.get('derived_artifacts_deleted') and not final.get('cleanup_pending'))
    output.update(reference_checked=reference_checked,
        all_certificates_checked=(reference_checked == len(ref['records']) and
            all(score['checked_clean'] == len(ref['records'])
                for score in arm_scores.values())),
        limitation='Fixed-reference cross-window score; runtime_target_met alone is not acceptance. B28+C5,current-source fuzz and memory gates remain separate.')
    output['comparison_reference'] = comparison_binding or binding
    return output


# Paper-target bounded controller regression ------------------------------

def independently_verified(row):
    if row['verdict'] == 'SAT':
        check = row.get('model_check') or {}
        return (check.get('result') == 'verified' and
                check.get('exit_code') == 0 and not check.get('timed_out'))
    check = row.get('proof_check') or {}
    return (check.get('result') == 'verified' and
            not check.get('timed_out') and check.get('stages') and
            all(stage.get('result') == 'verified'
                for stage in check['stages']))


def clean_censored_proof(row):
    check = row.get('proof_check') or {}
    stages = check.get('stages') or []
    return (row.get('verdict') == 'UNSAT' and check.get('timed_out') and
            check.get('result') != 'verified' and
            any(stage.get('timed_out') for stage in stages) and
            all(stage.get('result') == 'verified' or stage.get('timed_out')
                for stage in stages))


def collect_controller_paper():
    """Freeze the paper release's saved evidence; nothing is rerun."""
    snapshot = {
        'schema': 'tactix-controller-paper-baseline-v1',
        'target_source_commit': PAPER_COMMIT,
        'target_solver_sha256': PAPER_BINARY,
        'runtime_tolerance_fraction': 0.10,
        'regression_contract': 'paper release plus ten percent aggregate solver time',
        'sets': {},
    }
    for name, (folder_name, expected_count) in CONTROLLER_SETS.items():
        relative = Path('benchmarks/runs') / folder_name
        folder = ROOT / relative
        manifest_path = folder / 'manifest.json'
        manifest = json.loads(manifest_path.read_text())
        assert manifest['set'] == name
        assert len(manifest['arms']) == 1
        arm_name, arm = next(iter(manifest['arms'].items()))
        assert arm['source_commit'] == PAPER_COMMIT
        assert arm['sha256'] == PAPER_BINARY
        assert arm['arguments'] == ['--auto-proof']
        for pin, expected_hash in manifest['pin_sha256'].items():
            assert sha(folder / pin) == expected_hash, folder / pin
        assert sha(folder / 'matrix.tsv') == manifest['pin_sha256']['matrix.tsv']
        matrix = read_matrix(folder)
        assert len(matrix) == expected_count
        files = sorted((folder / 'results').glob('*.json'))
        assert len(files) == expected_count, (name, len(files))
        rows = []
        independently_checked = 0
        solve_times = []
        for task, path in enumerate(files):
            row = json.loads(path.read_text())
            expected = matrix[task]
            assert row['task'] == task
            assert row['arm'] == arm_name
            assert row['binary_sha256'] == PAPER_BINARY
            assert str(row['rank']) == expected['rank']
            assert all(row[key] == expected[key]
                       for key in ('instance', 'expected'))
            assert row['artifact_deleted'] and row['derived_artifacts_deleted']
            assert not row['cleanup_pending'] and not row.get('error')
            solve = row['solve']
            timed_out = bool(solve.get('timed_out'))
            if timed_out:
                assert row.get('verdict') == 'UNKNOWN'
                assert solve.get('exit_code') is None
                assert 0 <= solve['wall_seconds'] <= (
                    manifest['resource_contract']['solve_timeout'] + 1)
            else:
                assert row['verdict'] in ('SAT', 'UNSAT')
                assert row['verdict'] == row['expected'] and row['expected_match']
                assert solve['exit_code'] == SOLVER_EXIT[row['verdict']]
                assert independently_verified(row) or clean_censored_proof(row)
                independently_checked += independently_verified(row)
                solve_times.append(solve['wall_seconds'])
            retained = {key: row[key] for key in (
                'instance', 'rank', 'expected', 'verdict', 'expected_match',
                'valid', 'solve', 'model_check', 'proof_check',
                'certificate_sha256', 'certificate_bytes') if key in row}
            retained.update(
                source=str(relative / 'results' / path.name),
                source_sha256=sha(path),
                require_solver_retention=not timed_out,
            )
            rows.append(retained)
        paper_seconds = round(math.fsum(solve_times), 6)
        ceiling = round(paper_seconds * RUNTIME_FACTOR, 6)
        snapshot['sets'][name] = {
            'source_manifest': str(relative / 'manifest.json'),
            'source_manifest_sha256': sha(manifest_path),
            'matrix_sha256': sha(folder / 'matrix.tsv'),
            'resource_contract': manifest['resource_contract'],
            'pin_sha256': manifest['pin_sha256'],
            'input_sha256': manifest['input_sha256'],
            'expected_rows': expected_count,
            'independently_verified_rows': independently_checked,
            'paper_solver_seconds': paper_seconds,
            'runtime_tolerance_fraction': 0.10,
            'acceptance_runtime_seconds': ceiling,
            'rows': rows,
        }
    return snapshot


def verify_controller_paper(path=CONTROLLER_BASELINE):
    snapshot = collect_controller_paper()
    assert json.loads(path.read_text()) == snapshot
    rows = sum(value['expected_rows'] for value in snapshot['sets'].values())
    checked = sum(value['independently_verified_rows']
                  for value in snapshot['sets'].values())
    print(f'Verified paper controller target: {rows} rows, {checked} independently checked; source hashes match.')


def within_runtime_target(total, ceiling):
    """The paper target permits equality and no more than ten percent excess."""
    return ceiling is None or total <= ceiling


def score_target_path(path):
    try:
        return str(path.resolve().relative_to(ROOT))
    except ValueError:
        return str(path.resolve())


def proof_cost_comparison(candidate, baseline):
    """Describe the measured comparison without inventing censored completion."""
    elapsed = (candidate or {}).get('wall_seconds')
    original = (baseline or {}).get('wall_seconds')
    comparable = bool(baseline and baseline.get('result') == 'verified' and
                      not baseline.get('timed_out'))
    limit = original + max(0.30 * original, 20) if comparable else None
    if not candidate:
        status = 'missing'
    elif not isinstance(elapsed, (int, float)) or not math.isfinite(elapsed):
        status = 'invalid-measurement'
    elif proof_disposition(candidate, None)[0] == 'failed':
        status = 'checker-failure'
    elif not comparable:
        status = 'inconclusive'
    elif elapsed > limit:
        status = 'significant-slowdown'
    elif candidate.get('timed_out'):
        status = 'censored-within-bound'
    else:
        status = 'completed-within-bound'
    return dict(status=status, baseline_wall_seconds=original,
                candidate_wall_seconds=elapsed, allowed_completion_seconds=limit,
                baseline_censored=bool(baseline and baseline.get('timed_out')),
                candidate_censored=bool(candidate and candidate.get('timed_out')))


def candidate_arguments(arguments=()):
    """Keep this Default-policy gate on the whole-run PBP contract.

    Extensions are deliberately enumerated: arbitrary CLI policy/proof flags
    would silently change what the historical comparisons qualify. Since the
    command-line default became automatic selection, campaigns spell a leading
    `--pbp`; historical campaigns recorded none (their binaries' default).
    """
    values = list(arguments)
    proof = values[:1] if values[:1] == ['--pbp'] else []
    values = values[len(proof):]
    allowed = {
        '--default-affine-search=on', '--default-affine-search=off',
        '--default-graph-labeling=on', '--default-graph-labeling=off',
    }
    require(all(isinstance(value, str) and value in allowed for value in values),
            'unsupported qualified Default solver argument')
    families = [value.split('=', 1)[0] for value in values]
    require(len(families) == len(set(families)),
            'duplicate or conflicting qualified Default argument')
    return proof + values


def controller_prepare(prefix, release, solver_arguments=(), dsr_transport=None):
    arguments = candidate_arguments(solver_arguments)
    if arguments[:1] != ['--pbp']:
        arguments = ['--pbp', *arguments]
    require(release.is_relative_to(ROOT), 'candidate must be in repository')
    pin = json.loads((release.parent / 'PIN.json').read_text())
    require(pin['build_type'] == 'Release', 'production Release required')
    require(sha(release) == pin['binaries']['tactix'], 'candidate pin changed')
    verify_controller_paper()
    baseline = json.loads(CONTROLLER_BASELINE.read_text())
    campaigns = []
    for name, accepted in baseline['sets'].items():
        campaign = ROOT / 'benchmarks/runs' / (prefix + '-' + name)
        require(not campaign.exists(), 'campaign exists: ' + str(campaign))
        source = ROOT / accepted['source_manifest']
        require(sha(source) == accepted['source_manifest_sha256'], 'baseline changed')
        old = json.loads(source.read_text())
        require(len(old['arms']) == 1, 'baseline arm ambiguity')
        arm_name, old_arm = next(iter(old['arms'].items()))
        require(old_arm['arguments'] == ['--auto-proof'], 'unexpected baseline arguments')
        for instance, digest in accepted['input_sha256'].items():
            require(sha(ROOT / 'benchmarks/cache' / instance) == digest, 'input changed')
        start_campaign(campaign, dsr_transport)
        for filename in ('checkmodel', 'modelcheck.py', *CANONICAL_CHECKERS):
            original = source.parent / 'pins' / filename
            expected = accepted['pin_sha256']['pins/' + filename]
            require(sha(original) == expected, 'baseline checker changed: ' + filename)
            if filename in CANONICAL_CHECKERS:
                require(expected == CANONICAL_CHECKERS[filename], 'noncanonical checker')
            shutil.copy2(original, campaign / 'pins' / filename)
        shutil.copy2(release, campaign / 'pins/tactix')
        shutil.copy2(release.parent / 'PIN.json', campaign / 'pins/candidate-pin.json')
        shutil.copy2(CONTROLLER_BASELINE, campaign / 'pins/baseline.json')
        shutil.copy2(source.parent / 'matrix.tsv', campaign / 'matrix.tsv')
        require(sha(campaign / 'matrix.tsv') == accepted['matrix_sha256'], 'matrix changed')
        contract = dict(accepted['resource_contract'])
        contract.update(checker_timeout=CHECK_TIMEOUT, checker_timeout_overrides={},
                        array=f"0-{accepted['expected_rows'] - 1}", throttle=None,
                        node_constraint=None)
        (campaign / 'array.sbatch').write_text(array_batch(
            campaign, shebang='#!/bin/bash', job='controller-' + name,
            partition=contract['partition'], cpus=contract['cpus_per_task'],
            memory_mib=contract['memory_mib'], time='01:10:00',
            array=contract['array'],
            environment='export PYTHONDONTWRITEBYTECODE=1\n'))
        manifest = dict(schema='tactix-controller-regression-v1', set=name,
            created_at=dt.datetime.now(dt.timezone.utc).isoformat(),
            baseline=dict(path=str(CONTROLLER_BASELINE.relative_to(ROOT)), sha256=sha(CONTROLLER_BASELINE)),
            baseline_manifest=dict(path=str(source.relative_to(ROOT)), sha256=sha(source)),
            arms={arm_name: dict(path='pins/tactix', sha256=sha(release),
                source_commit=pin['source_commit'], source_build=str(release.parent.relative_to(ROOT)),
                arguments=arguments, certificate='pbp')},
            resource_contract=contract, input_sha256=accepted['input_sha256'],
            collect_runtime_context=old.get('collect_runtime_context', False),
            scratch_policy='anonymous-local-files-v1',
            regression_contract='paper release plus ten percent aggregate solver time',
            limitation='Paper-target comparison; clean censored proofs are not independently verified.',
            acceptance_runtime_seconds=accepted['acceptance_runtime_seconds'])
        manifest['pin_sha256'] = pin_hashes(campaign)
        (campaign / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        campaigns.append(str(campaign))
    return campaigns


def proof_disposition(candidate, baseline):
    """Failures dominate clean timeout; completed baseline supplies a cost bound."""
    if not candidate:
        return 'missing', ['missing proof-check outcome']
    stages = candidate.get('stages', [])
    if not stages or any(s.get('result') != 'verified' and not s.get('timed_out')
                         for s in stages):
        return 'failed', ['checker rejection/error or missing stage evidence']
    elapsed = candidate.get('wall_seconds')
    if not finite_seconds(elapsed):
        return 'failed', ['invalid checker elapsed time']
    if candidate.get('timed_out'):
        if candidate.get('result') == 'verified' or not any(s.get('timed_out') for s in stages):
            return 'failed', ['contradictory checker timeout evidence']
        disposition = 'censored'
    elif candidate.get('result') == 'verified' and all(s['result'] == 'verified' for s in stages):
        disposition = 'verified'
        if stages[-1]['name'] != FINAL_VERIFIER.get(candidate.get('pipeline')):
            return 'failed', ['missing final independent verifier']
    else:
        return 'failed', ['checker did not verify or cleanly time out']
    errors = []
    if elapsed > CHECK_TIMEOUT + 1:
        errors.append('proof pipeline exceeded shared deadline')
    if baseline and baseline.get('result') == 'verified' and not baseline.get('timed_out'):
        original = baseline['wall_seconds']
        if elapsed > original + max(0.30 * original, 20):
            errors.append('significant proof-check cost regression')
    return disposition, errors


def score_row(row, accepted, contract):
    errors = []
    if row.get('error') or row.get('cleanup_pending') or not row.get('artifact_deleted') or not row.get('derived_artifacts_deleted'):
        errors.append('infrastructure error or incomplete scratch cleanup')
    solve = row.get('solve') or {}
    verdict = row.get('verdict')
    if solve.get('timed_out'):
        elapsed = solve.get('wall_seconds')
        if (verdict != 'UNKNOWN' or solve.get('exit_code') is not None or
                not finite_seconds(elapsed) or
                elapsed > contract['solve_timeout'] + 1):
            errors.append('invalid solver-timeout evidence')
        if accepted['require_solver_retention']:
            errors.append('lost retained solve')
        # A saved solver-timeout exception may remain a clean timeout.
        if row.get('proof_check'):
            _, proof_errors = proof_disposition(row['proof_check'], accepted.get('proof_check'))
            errors.extend(proof_errors)
        return 'solver-timeout', errors
    if verdict not in ('SAT', 'UNSAT') or solve.get('exit_code') != SOLVER_EXIT.get(verdict):
        errors.append('solver failure or inconsistent exit/verdict')
    if verdict != accepted['expected'] or not row.get('expected_match'):
        errors.append('incorrect verdict')
    elapsed, rss = solve.get('wall_seconds'), solve.get('peak_rss_kb')
    if not finite_seconds(elapsed) or elapsed > contract['solve_timeout']:
        errors.append('solve-time contract violated')
    if not isinstance(rss, (int, float)) or not math.isfinite(rss) or rss <= 0 or rss > contract['memory_mib'] * 1024:
        errors.append('memory contract violated or measurement missing')
    if verdict == 'SAT':
        model = row.get('model_check') or {}
        if model.get('result') != 'verified' or model.get('exit_code') != 0 or model.get('timed_out'):
            errors.append('model not independently verified')
        return 'model-verified', errors
    disposition, proof_errors = proof_disposition(row.get('proof_check'), accepted.get('proof_check'))
    return disposition, errors + proof_errors


def controller_score(campaign, target_baseline=None):
    manifest = json.loads((campaign / 'manifest.json').read_text())
    require(manifest['schema'] == 'tactix-controller-regression-v1', 'wrong manifest')
    require(len(manifest['arms']) == 1, 'candidate arm ambiguity')
    arm = next(iter(manifest['arms'].values()))
    wire = arm.get('arguments')
    require(isinstance(wire, list) and
            wire == candidate_arguments(wire) and arm.get('certificate') == 'pbp',
            'candidate certificate/argument contract changed')
    verify_pins(campaign, manifest, 'campaign pin changed: ')
    if target_baseline is None:
        baseline_path = campaign / 'pins/baseline.json'
        require(sha(baseline_path) == manifest['baseline']['sha256'], 'campaign baseline changed')
        baseline = json.loads(baseline_path.read_text())
        target_kind = 'campaign-pinned'
    else:
        baseline_path = target_baseline
        baseline = json.loads(baseline_path.read_text())
        target_kind = 'paper-release-plus-10-percent'
    require(manifest['set'] in baseline['sets'], 'set absent from target baseline')
    accepted = baseline['sets'][manifest['set']]
    contract = manifest['resource_contract']
    require(contract['checker_timeout'] == CHECK_TIMEOUT and
            not contract.get('checker_timeout_overrides') and
            contract['throttle'] is None and contract['node_constraint'] is None,
            'bounded/full-width regression contract changed')
    for key in ('partition', 'cpus_per_task', 'memory_mib', 'solve_timeout', 'one_solver_per_task'):
        require(contract[key] == accepted['resource_contract'][key], 'resource mismatch: ' + key)
    matrix = read_matrix(campaign)
    require(len(matrix) == accepted['expected_rows'], 'matrix row count')
    require(sha(campaign / 'matrix.tsv') == accepted['matrix_sha256'], 'target matrix mismatch')
    require(manifest['input_sha256'] == accepted['input_sha256'], 'target inputs mismatch')
    by_instance = {row['instance']: row for row in accepted['rows']}
    counts, failures, missing, times, comparisons = Counter(), [], [], [], []
    for task, expected in enumerate(matrix):
        path = campaign / 'results' / f'{task:03d}.json'
        if not path.exists():
            missing.append(task)
            continue
        row = json.loads(path.read_text())
        require(row['task'] == task and str(row['rank']) == expected['rank'] and
                all(row[k] == expected[k] for k in ('arm', 'tier', 'instance', 'expected')), 'row identity mismatch')
        require(row['binary_sha256'] == manifest['arms'][expected['arm']]['sha256'], 'result binary mismatch')
        disposition, errors = score_row(row, by_instance[row['instance']], manifest['resource_contract'])
        counts[disposition] += 1
        if row.get('proof_check'):
            comparison = proof_cost_comparison(row['proof_check'],
                by_instance[row['instance']].get('proof_check'))
            comparison.update(task=task, instance=row['instance'])
            comparisons.append(comparison)
        if errors:
            failures.append(dict(task=task, instance=row['instance'], errors=errors))
        if not row.get('solve', {}).get('timed_out') and isinstance(row.get('solve', {}).get('wall_seconds'), (int, float)):
            times.append(row['solve']['wall_seconds'])
    total = math.fsum(times)
    ceiling = (manifest['acceptance_runtime_seconds'] if target_baseline is None
               else accepted['acceptance_runtime_seconds'])
    if not missing and not within_runtime_target(total, ceiling):
        failures.append(dict(errors=['aggregate solver-time ceiling exceeded']))
    require(len(list((campaign / 'results').glob('*.json'))) == len(matrix) - len(missing), 'unexpected result files')
    return dict(schema='tactix-controller-regression-score-v1', set=manifest['set'],
                manifest_sha256=sha(campaign / 'manifest.json'),
                scorer_sha256=sha(Path(__file__)),
                target=target_kind, target_baseline_path=score_target_path(baseline_path),
                target_baseline_sha256=sha(baseline_path),
                target_source_commit=baseline.get('target_source_commit'),
                runtime_tolerance_fraction=accepted.get('runtime_tolerance_fraction'),
                target_solver_seconds=accepted.get('paper_solver_seconds'),
                accepted=not missing and not failures, expected_rows=len(matrix),
                dispositions=dict(counts), missing=missing, failures=failures,
                solver_seconds=total, solver_seconds_ceiling=ceiling,
                independently_verified=counts['model-verified'] + counts['verified'],
                censored_proofs=counts['censored'],
                proof_cost_dispositions=dict(Counter(c['status'] for c in comparisons)),
                proof_cost_comparisons=comparisons,
                limitation='Bounded regression acceptance does not certify censored proofs.')


# Full Tier D against the paper release -----------------------------------

def result_inventory(paths):
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(f'{path.name} {sha(path)}\n'.encode())
    return digest.hexdigest()


def verify_manifest(campaign):
    manifest = json.loads((campaign / 'manifest.json').read_text())
    verify_pins(campaign, manifest, f'pin changed: {campaign.name}/')
    return manifest


def recheck_campaigns(recheck):
    """Normalize one or more direct rechecks in overlay order."""
    if recheck is None:
        return []
    if isinstance(recheck, (str, Path)):
        return [Path(recheck)]
    return list(recheck)


def checked_result(row, result, contract):
    """Return (status, solve seconds, errors) for one immutable result."""
    errors = []
    for key in ('instance', 'expected'):
        if result.get(key) != row[key]:
            errors.append('result identity mismatch: ' + key)
    if (result.get('error') or result.get('cleanup_pending') or
            result.get('artifact_deleted') is not True or
            result.get('derived_artifacts_deleted') is not True):
        errors.append('infrastructure error or incomplete scratch cleanup')
    solve = result.get('solve') or {}
    seconds = solve.get('wall_seconds')
    if not finite_seconds(seconds) or seconds > contract['solve_timeout'] + 1:
        errors.append('solve-time contract violated')
        seconds = None
    if solve.get('timed_out'):
        if (result.get('verdict') != 'UNKNOWN' or solve.get('exit_code') is not None or
                result.get('model_check') is not None or result.get('proof_check') is not None):
            errors.append('invalid solver-timeout evidence')
        return 'solver-timeout', seconds, errors

    verdict = result.get('verdict')
    rss = solve.get('peak_rss_kb')
    if (not isinstance(rss, (int, float)) or not math.isfinite(rss) or
            rss <= 0 or rss > contract['memory_mib'] * 1024):
        errors.append('memory contract violated or measurement missing')
    if (verdict == 'UNKNOWN' and solve.get('exit_code') == 137 and
            result.get('model_check') is None and result.get('proof_check') is None):
        return 'solver-memout', seconds, errors
    if verdict not in ('SAT', 'UNSAT') or solve.get('exit_code') != SOLVER_EXIT.get(verdict):
        errors.append('solver failure or inconsistent exit/verdict')
        return 'solver-failure', seconds, errors
    if row['expected'] not in (verdict, 'UNKNOWN') or result.get('expected_match') is not True:
        errors.append('incorrect verdict')
    if verdict == 'SAT':
        check = result.get('model_check') or {}
        if (result.get('valid') is not True or check.get('result') != 'verified' or
                check.get('exit_code') != 0 or check.get('timed_out')):
            errors.append('SAT model not independently verified')
            return 'invalid-SAT', seconds, errors
        return 'verified-SAT', seconds, errors

    check = result.get('proof_check') or {}
    check_seconds = check.get('wall_seconds')
    if not finite_seconds(check_seconds) or check_seconds > contract['checker_timeout'] + 1:
        errors.append('proof-check time contract violated')
    if result.get('valid') is True and check.get('result') == 'verified' and not check.get('timed_out'):
        stages = check.get('stages') or []
        if not stages or any(stage.get('result') != 'verified' for stage in stages):
            errors.append('verified proof lacks verified stage evidence')
            return 'invalid-UNSAT', seconds, errors
        if stages[-1].get('name') != FINAL_VERIFIER.get(check.get('pipeline')):
            errors.append('verified proof lacks final independent verifier')
            return 'invalid-UNSAT', seconds, errors
        return 'verified-UNSAT', seconds, errors
    if result.get('valid') is True:
        errors.append('censored proof mislabeled valid')
    failure = check_failure_class(check)
    if failure not in ('timeout', 'memout'):
        errors.append('UNSAT proof rejected or checker failed unclearly')
        return 'invalid-UNSAT', seconds, errors
    return 'censored-UNSAT-' + failure, seconds, errors


def substituted_results(campaign, recheck=None):
    manifest = verify_manifest(campaign)
    require(list(manifest.get('arms', {})) == ['tactix'],
            'campaign must contain exactly the tactix arm')
    arm = manifest['arms']['tactix']
    # Whole-run PBP (flagless before the automatic default, `--pbp` after it)
    # or the automatic selection of the paper baseline and of later defaults.
    require((arm.get('arguments'), arm.get('certificate')) in
            (([], 'pbp'), (['--pbp'], 'pbp'), (['--auto-proof'], 'auto')),
            'candidate proof contract changed')
    matrix = read_matrix(campaign)
    require(len(matrix) == 1166, 'full Tier D must contain 1,166 rows')
    require([int(row['task']) for row in matrix] == list(range(1166)),
            'full Tier D task order changed')
    results = {}
    for row in matrix:
        task = int(row['task'])
        path = campaign / 'results' / f'{task:03d}.json'
        if path.is_file():
            result = json.loads(path.read_text())
            require(result.get('task') == task and int(result.get('rank')) == int(row['rank']),
                    'base result task/rank identity mismatch')
            require(result.get('arm') == 'tactix' and
                    result.get('binary_sha256') == arm['sha256'],
                    'base result solver identity mismatch')
            results[task] = result
    for recheck in recheck_campaigns(recheck):
        re_manifest = verify_manifest(recheck)
        authority = re_manifest.get('recheck_source') or {}
        require(authority.get('manifest_sha256') == sha(campaign / 'manifest.json'),
                'recheck does not bind candidate campaign')
        require(re_manifest['resource_contract']['memory_mib'] == 65536 and
                re_manifest['resource_contract']['solve_timeout'] == 5000 and
                re_manifest['resource_contract']['checker_timeout'] == 28800,
                'recheck resource contract changed')
        require(re_manifest.get('arms') == manifest.get('arms'),
                'recheck solver identity changed')
        for row in read_matrix(recheck):
            local = int(row['task'])
            source_task = int(row['source_task'])
            source_path = campaign / 'results' / f'{source_task:03d}.json'
            expected_hash = authority.get('selected_result_sha256', {}).get(
                f'results/{source_task:03d}.json')
            require(expected_hash and sha(source_path) == expected_hash,
                    'recheck source result changed')
            path = recheck / 'results' / f'{local:03d}.json'
            require(path.is_file(), f'recheck result missing: {recheck.name}/{local:03d}')
            result = json.loads(path.read_text())
            require(result.get('task') == local and
                    result.get('instance') == row['instance'],
                    'recheck result identity mismatch')
            require(result.get('arm') == 'tactix' and
                    result.get('binary_sha256') == arm['sha256'],
                    'recheck result solver identity mismatch')
            results[source_task] = result
    return manifest, matrix, results


def collect_paper_target():
    manifest, matrix, results = substituted_results(TIERD_BASE, TIERD_RECHECK)
    arm = next(iter(manifest['arms'].values()))
    require(manifest['source_commit'] == PAPER_COMMIT and arm['sha256'] == PAPER_BINARY,
            'paper release identity changed')
    require(len(results) == 1166, 'paper target results incomplete')
    recheck_by_source = {int(row['source_task']): int(row['task'])
                         for row in read_matrix(TIERD_RECHECK)}
    rows, raw, verified = [], [], []
    counts = Counter()
    for row in matrix:
        task = int(row['task'])
        result = results[task]
        contract = (json.loads((TIERD_RECHECK / 'manifest.json').read_text())['resource_contract']
                    if task in recheck_by_source
                    else manifest['resource_contract'])
        status, seconds, errors = checked_result(row, result, contract)
        require(not errors, f'paper target row {task} invalid: {errors}')
        solved = status.startswith(('verified-', 'censored-'))
        valid = status.startswith('verified-')
        raw.append(seconds if solved else 10000)
        verified.append(seconds if valid else 10000)
        counts[status] += 1
        source_campaign = TIERD_RECHECK if task in recheck_by_source else TIERD_BASE
        source_task = recheck_by_source.get(task, task)
        source_path = source_campaign / 'results' / f'{source_task:03d}.json'
        rows.append({
            'task': task, 'rank': int(row['rank']), 'instance': row['instance'],
            'expected': row['expected'], 'status': status,
            'solved': solved, 'independently_verified': valid,
            'solve_wall_seconds': seconds,
            'source': str(source_path.relative_to(ROOT)),
            'source_sha256': sha(source_path),
        })
    raw_par2 = round(math.fsum(raw), 6)
    verified_par2 = round(math.fsum(verified), 6)
    return {
        'schema': 'tactix-tierd-paper-regression-baseline-v1',
        'target_source_commit': PAPER_COMMIT,
        'target_solver_sha256': PAPER_BINARY,
        'runtime_tolerance_fraction': 0.10,
        'source_base_manifest': str((TIERD_BASE / 'manifest.json').relative_to(ROOT)),
        'source_base_manifest_sha256': sha(TIERD_BASE / 'manifest.json'),
        'source_base_results_inventory_sha256': result_inventory(
            (TIERD_BASE / 'results').glob('*.json')),
        'source_recheck_manifest': str((TIERD_RECHECK / 'manifest.json').relative_to(ROOT)),
        'source_recheck_manifest_sha256': sha(TIERD_RECHECK / 'manifest.json'),
        'source_recheck_results_inventory_sha256': result_inventory(
            (TIERD_RECHECK / 'results').glob('*.json')),
        'resource_contract': manifest['resource_contract'],
        'matrix_sha256': sha(TIERD_BASE / 'matrix.tsv'),
        'counts': dict(counts),
        'paper_raw_solver_par2': raw_par2,
        'paper_verified_par2': verified_par2,
        'acceptance_raw_solver_par2': round(raw_par2 * RUNTIME_FACTOR, 6),
        'acceptance_verified_par2': round(verified_par2 * RUNTIME_FACTOR, 6),
        'rows': rows,
    }


def load_verified_baseline(path=TIERD_BASELINE):
    expected = collect_paper_target()
    require(json.loads(path.read_text()) == expected,
            'frozen paper Tier-D target differs from source evidence')
    return expected


def tierd_score(campaign, recheck=None, baseline_path=TIERD_BASELINE):
    baseline = load_verified_baseline(baseline_path)
    rechecks = recheck_campaigns(recheck)
    manifest, matrix, results = substituted_results(campaign, rechecks)
    contract = manifest['resource_contract']
    for key, value in (('partition', 'cpu-q'), ('cpus_per_task', 4),
                       ('memory_mib', 32768), ('solve_timeout', 5000),
                       ('checker_timeout', 28800)):
        require(contract[key] == value, 'candidate resource mismatch: ' + key)
    require(sha(campaign / 'matrix.tsv') == baseline['matrix_sha256'],
            'candidate Tier-D matrix differs from paper target')
    target = {row['task']: row for row in baseline['rows']}
    counts, failures, missing, raw, verified = Counter(), [], [], [], []
    recheck_contracts = {}
    for recheck_campaign in rechecks:
        re_contract = json.loads((recheck_campaign / 'manifest.json').read_text())['resource_contract']
        for recheck_row in read_matrix(recheck_campaign):
            recheck_contracts[int(recheck_row['source_task'])] = re_contract
    for row in matrix:
        task = int(row['task'])
        if task not in results:
            missing.append(task)
            raw.append(10000); verified.append(10000)
            continue
        row_contract = recheck_contracts.get(task, contract)
        status, seconds, errors = checked_result(row, results[task], row_contract)
        counts[status] += 1
        solved = status.startswith(('verified-', 'censored-'))
        valid = status.startswith('verified-')
        raw.append(seconds if solved else 10000)
        verified.append(seconds if valid else 10000)
        if target[task]['solved'] and not solved:
            errors.append('lost paper solve')
        if target[task]['independently_verified'] and not valid:
            errors.append('lost paper independent verification')
        if errors:
            failures.append({'task': task, 'instance': row['instance'],
                             'status': status, 'errors': errors})
    raw_par2 = math.fsum(raw)
    verified_par2 = math.fsum(verified)
    if not missing and raw_par2 > baseline['acceptance_raw_solver_par2']:
        failures.append({'errors': ['raw solver PAR-2 exceeds paper plus ten percent']})
    if not missing and verified_par2 > baseline['acceptance_verified_par2']:
        failures.append({'errors': ['verified PAR-2 exceeds paper plus ten percent']})
    return {
        'schema': 'tactix-tierd-paper-regression-score-v1',
        'campaign': str(campaign.relative_to(ROOT)),
        'campaign_manifest_sha256': sha(campaign / 'manifest.json'),
        'scorer_sha256': sha(Path(__file__)),
        'recheck_campaign': (str(rechecks[0].relative_to(ROOT))
                             if len(rechecks) == 1 else None),
        'recheck_campaigns': [str(path.relative_to(ROOT)) for path in rechecks],
        'baseline_sha256': sha(baseline_path),
        'target_source_commit': baseline['target_source_commit'],
        'runtime_tolerance_fraction': 0.10,
        'accepted': not missing and not failures,
        'expected_rows': 1166, 'published_rows': len(results),
        'missing': missing, 'counts': dict(counts), 'failures': failures,
        'raw_solver_par2': round(raw_par2, 6),
        'raw_solver_par2_ceiling': baseline['acceptance_raw_solver_par2'],
        'verified_par2': round(verified_par2, 6),
        'verified_par2_ceiling': baseline['acceptance_verified_par2'],
        'paper_raw_solver_par2': baseline['paper_raw_solver_par2'],
        'paper_verified_par2': baseline['paper_verified_par2'],
        'limitation': 'The seven paper solver successes without independent proof verification remain explicit censored target rows.',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    p = commands.add_parser('fixed-prepare')
    p.add_argument('campaign', type=Path)
    for arg in ('authority', 'release', 'bc'):
        p.add_argument('--' + arg, type=Path, required=True)
    p.add_argument('--dsr-transport', type=Path)
    p.add_argument('--reference', type=Path, default=REFERENCE)
    p.add_argument('--runtime-target-seconds', type=float)
    p.add_argument('--checker-pins', type=Path, required=True,
                   help='frozen PBP/DRAT checker bundle')
    p = commands.add_parser('fixed-score')
    p.add_argument('campaign', type=Path)
    p.add_argument('--reference', type=Path)
    p.add_argument('--runtime-target-seconds', type=float)
    p = commands.add_parser('fixed-submit')
    p.add_argument('campaign', type=Path)
    p = commands.add_parser('controller-prepare')
    p.add_argument('--prefix', required=True)
    p.add_argument('--release', type=Path, required=True)
    p.add_argument('--dsr-transport', type=Path)
    p.add_argument('--solver-argument', action='append', default=[],
                   help='append a qualified Default affine-search or graph-labeling setting')
    for name in ('controller-score', 'controller-score-paper'):
        p = commands.add_parser(name)
        p.add_argument('campaign', type=Path)
        p.add_argument('--output', type=Path)
        p.add_argument('--replace-output', action='store_true')
    p = commands.add_parser('controller-freeze',
                            help='freeze (or --verify) the paper controller target')
    p.add_argument('--output', type=Path, default=CONTROLLER_BASELINE)
    p.add_argument('--verify', action='store_true')
    p = commands.add_parser('tierd-freeze')
    p.add_argument('--output', type=Path, default=TIERD_BASELINE)
    p = commands.add_parser('tierd-verify')
    p.add_argument('--baseline', type=Path, default=TIERD_BASELINE)
    p = commands.add_parser('tierd-score')
    p.add_argument('campaign', type=Path)
    p.add_argument('--recheck', type=Path, action='append', default=[],
                   help='repeatable direct recheck; later campaigns override overlapping source tasks')
    p.add_argument('--baseline', type=Path, default=TIERD_BASELINE)
    p.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.command == 'fixed-prepare':
        fixed_prepare(args.campaign.resolve(), args.authority.resolve(),
                      args.release.resolve(), args.bc.resolve(),
                      args.reference.resolve(), args.runtime_target_seconds,
                      args.checker_pins.resolve(), args.dsr_transport)
        print(args.campaign)
    elif args.command == 'fixed-score':
        print(json.dumps(fixed_score(
            args.campaign.resolve(),
            args.reference.resolve() if args.reference else None,
            args.runtime_target_seconds), indent=2))
    elif args.command == 'fixed-submit':
        submit(args.campaign.resolve())
    elif args.command == 'controller-prepare':
        require('/' not in args.prefix and args.prefix not in ('', '.', '..'), 'invalid prefix')
        print(json.dumps(controller_prepare(args.prefix, args.release.resolve(),
                                            args.solver_argument, args.dsr_transport), indent=2))
    elif args.command in ('controller-score', 'controller-score-paper'):
        target = None
        if args.command == 'controller-score-paper':
            verify_controller_paper()
            target = CONTROLLER_BASELINE
        result = controller_score(args.campaign.resolve(), target)
        if args.output:
            write_score(args.output, result, args.replace_output)
        print(json.dumps(result, indent=2))
        return 0 if result['accepted'] else 1
    elif args.command == 'controller-freeze':
        if args.verify:
            verify_controller_paper(args.output)
        else:
            snapshot = collect_controller_paper()
            args.output.parent.mkdir(parents=True, exist_ok=True)
            with args.output.open('x') as target:
                json.dump(snapshot, target, indent=2, sort_keys=True)
                target.write('\n')
            print(args.output)
    elif args.command == 'tierd-freeze':
        require(not args.output.exists(), 'baseline output already exists')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(collect_paper_target(), indent=2,
                                          sort_keys=True) + '\n')
        print(args.output)
    elif args.command == 'tierd-verify':
        baseline = load_verified_baseline(args.baseline)
        print(f"Verified paper Tier-D target: {len(baseline['rows'])} rows; "
              f"verified PAR-2 {baseline['paper_verified_par2']:.3f}.")
    else:
        result = tierd_score(args.campaign.resolve(),
                             [path.resolve() for path in args.recheck],
                             args.baseline.resolve())
        if args.output:
            write_score(args.output, result)
        print(json.dumps(result, indent=2))
        return 0 if result['accepted'] else 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
