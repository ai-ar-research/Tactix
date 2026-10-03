#!/usr/bin/env python3
"""Audit saved portfolio misses without running a solver or checker.

Writes a NEW evidence directory, never updates a frozen report. Run from any
directory: python3 -B tools/bench/winner_attribution.py --output <new-directory>.
Handler attribution is from a separate, unchecked historical run; it is never
promoted to the handler of the later checked run.
"""
import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
import score_tierd_full as scorer

ROOT = Path(__file__).resolve().parents[2]
PORTFOLIO = ROOT / 'report-to-andrew/20261003T164617Z-tierd-portfolio-misses'
OLD = ROOT / 'benchmarks/runs/20260819-tierd-lymphosat-modlog'
CSV_SHA = 'b2444220c04a4e1a05bbe3d21c4bc73d2e119bad625fb31a9a3ac7d773c0b4f6'
ARMS = ('kissat-sc2024', 'lymphosat', 'satsuma-kissat', 'ae-kissat-mab', 'tactix')
FIXED36 = (82,147,190,226,263,291,323,343,367,401,453,479,489,497,511,540,
           541,569,627,630,654,744,754,762,781,887,896,899,941,991,1004,
           1075,1129,1144,1161,1163)
MAB8 = (97,410,499,557,1009,1040,1085,1118)


def load(path):
    return json.loads(path.read_text())


def require(condition, message):
    scorer.require(condition, message)


def write_json(path, value):
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    require(output.is_relative_to(ROOT), 'evidence must remain in repository')
    require(not output.exists(), 'refusing to overwrite an existing evidence directory')
    inputs = {}

    def bind(path, expected=None):
        digest = scorer.sha(path)
        if expected is not None:
            require(digest == expected, 'changed evidence: ' + str(path))
        inputs[str(path.relative_to(ROOT))] = digest
        return digest

    manifest = load(PORTFOLIO / 'manifest.json')
    bind(PORTFOLIO / 'manifest.json')
    bind(PORTFOLIO / 'portfolio-misses.csv', CSV_SHA)
    require(manifest['csv_sha256'] == CSV_SHA, 'wrong portfolio CSV authority')
    for rel, digest in manifest['inputs_sha256'].items():
        bind(ROOT / rel, digest)
    with (PORTFOLIO / 'portfolio-misses.csv').open(newline='') as stream:
        targets = list(csv.DictReader(stream))
    ranks = [int(row['rank']) for row in targets]
    require(ranks == sorted(FIXED36 + MAB8), 'target set changed')

    # The scorer verifies launch, archived dataset, all pin hashes and limits.
    campaigns = {name: scorer.read_campaign(name)
                 for name in sorted(set(scorer.CAMPAIGNS.values()))}
    matrices = {}
    for arm in ARMS:
        path, _, matrix = campaigns[scorer.CAMPAIGNS[arm]]
        bind(path / 'manifest.json')
        bind(path / 'launch.json')
        if (path / 'matrix.tsv').exists():
            bind(path / 'matrix.tsv')
        matrices[arm] = {int(row['rank']): row for row in matrix if row['arm'] == arm}
    bind(scorer.ARCHIVE)
    archive = load(scorer.ARCHIVE)
    old_manifest = load(OLD / 'manifest.json')
    old_results = load(OLD / 'results.json')
    bind(OLD / 'manifest.json', old_results['manifest_sha256'])
    bind(OLD / 'results.json')
    bind(scorer.ARCHIVE, old_manifest['base_manifest_sha256'])
    require(old_results['complete'] and old_results['result_count'] == 1166,
            'historical attribution aggregate incomplete')
    old_by_instance = {row['instance']: row for row in old_results['results']}
    require(len(old_by_instance) == 1166, 'duplicate historical attribution instance')

    # Bind audited reference sources to the actual checked campaign's pin.
    source_bindings = []
    lympho_path, lympho_manifest, _ = campaigns[scorer.CAMPAIGNS['lymphosat']]
    owner_root = ROOT / 'winners/lymphosat-sc2026/green/lymphosat/src/solver'
    for rel in (
        'core/main.cpp', 'order.json',
        'solvers/heule-nol/solver/solver.cpp',
        'solvers/sorting-networks/solver/solver.cpp',
        'solvers/antibandwidth/solver/solver.cpp',
        'solvers/ramsey-numbers/solver/solver',
        'solvers/ramsey-numbers/solver/README.md',
        'solvers/fallback/solver/src/src/search.c',
        'solvers/fallback/solver/src/src/proof.c',
    ):
        pin_rel = 'pins/lymphosat/solver/' + rel
        digest = bind(lympho_path / pin_rel, lympho_manifest['pin_sha256'][pin_rel])
        source = owner_root / rel
        bind(source, digest)
        source_bindings.append(dict(source=str(source.relative_to(ROOT)),
                                    pin=str((lympho_path / pin_rel).relative_to(ROOT)),
                                    sha256=digest))
    for rel in (
        'src/rules/lucky/onehot_model.cpp', 'src/rules/lucky/signed_cardinality_proof.cpp',
        'src/strategy/local_search.hpp', 'src/kernel/kernel.cpp',
        'src/policies/ops/local_search/local_search.cpp',
        'src/policies/ops/walk/walk.cpp', 'src/policies/default_policy.cpp',
        'docs/scheduling-parity-ledger.md', 'docs/winner-integration-plan.md',
        'tools/bench/score_tierd_full.py', 'tools/bench/winner_attribution.py',
    ):
        bind(ROOT / rel)

    rows = []
    raw_records = []
    for target in targets:
        rank = int(target['rank'])
        archived = archive['instances'][rank - 1]
        require((target['instance'], target['expected_verdict']) ==
                (archived['rel'], archived['expected']), 'archive identity mismatch')
        row = dict(target)
        row['checkpoint'] = 'fixed36' if rank in FIXED36 else 'mab_only8'
        require((rank in FIXED36) == bool(set(target['checked_solvers'].split(';')) &
                set(ARMS[:3])), 'checkpoint membership mismatch')
        for arm in ARMS:
            record = load(ROOT / target[arm + '_source_record'])
            status, seconds, _ = scorer.classify(matrices[arm][rank], record, arm)
            require(status == target[arm + '_status'], 'saved status mismatch')
            require(seconds == float(target[arm + '_solve_seconds']), 'timing mismatch')
            require((status.startswith('checked_')) == (target[arm + '_checked'] == 'true'),
                    'checked flag mismatch')
            require(record['verdict'] == target[arm + '_verdict'], 'verdict mismatch')
            if status.startswith('checked_'):
                require(record['verdict'] == target['verdict'], 'checked verdict disagreement')
            check = (record.get('proof_check') or {})
            row[arm + '_certificate_bytes'] = record.get('certificate_bytes', '')
            row[arm + '_declared_format'] = record.get('certificate_format', '')
            row[arm + '_content_route'] = check.get('certificate_content_route', '')
            row[arm + '_certificate_kind'] = ('model' if record['verdict'] == 'SAT' else
                'completed-unsat-proof' if record['verdict'] == 'UNSAT' else 'partial-or-unknown')
            if arm == 'lymphosat':
                row['latest_lymphosat_matcher_messages'] = json.dumps([
                    line for line in record.get('solver_stderr_tail', [])
                    if 'accepted formula' in line or 'c detected ' in line])
            raw_records.append(record)
        old = old_by_instance[target['instance']]
        attr = old['attribution']
        row['historical_lymphosat_handler'] = attr['responsible'] or 'unknown'
        row['historical_lymphosat_verdict'] = old['verdict']
        row['historical_lymphosat_attempts'] = json.dumps(attr['attempts'], sort_keys=True)
        row['historical_lymphosat_seconds'] = old['solve']['wall_seconds']
        row['historical_attribution_limit'] = 'separate run; not independently checked; latest final handler unlogged'
        # These are diagnostic leads, deliberately never causal attributions.
        if rank == 401:
            lead = 'general hypergraph coloring; exclude embedded 473-group witness'
        elif rank == 497:
            lead = 'general structural phase construction; exclude fixed sorting-network phase lists'
        elif rank == 630:
            lead = 'validated vertex/color symmetry and conditional cardinality reasoning; DSR feasibility first'
        elif rank == 991:
            lead = 'graph-labeling constructor with bounded repair and separately selectable phase guidance'
        elif rank == 1161:
            lead = 'latest final handler unknown; matcher acceptance does not establish specialist success'
        elif target['tactix_miss_reason'] != 'solver_timeout':
            lead = 'proof/checker cost and failure diagnosis; byte count alone is not causal'
        elif rank in MAB8:
            lead = 'search-policy mechanism attribution; previous global MAB integration failed regression gates'
        elif target['satsuma-kissat_checked'] == 'true':
            lead = 'symmetry/search interaction hypothesis; no saved ablation establishes cause'
        else:
            lead = 'native search/phase/work divergence; no missing specialist established by saved evidence'
        row['next_mechanism_investigation'] = lead
        rows.append(row)

    summary = dict(
        targets=len(rows), selected_records_reclassified=len(raw_records),
        original_manifest_input_hashes_verified=len(manifest['inputs_sha256']),
        campaign_pin_hashes_verified=sum(len(m['pin_sha256']) for _,m,_ in campaigns.values()),
        source_bindings=source_bindings,
        checkpoints={name: dict(count=sum(r['checkpoint'] == name for r in rows),
            verdicts=dict(Counter(r['verdict'] for r in rows if r['checkpoint'] == name)),
            miss_reasons=dict(Counter(r['tactix_miss_reason'] for r in rows if r['checkpoint'] == name)))
            for name in ('fixed36', 'mab_only8')},
        checked_lymphosat_historical_handlers=dict(Counter(r['historical_lymphosat_handler']
            for r in rows if r['lymphosat_checked'] == 'true')),
        checked_lymphosat_unsat_larger_certificates=[int(r['rank']) for r in rows
            if r['verdict'] == 'UNSAT' and r['lymphosat_checked'] == 'true'
            and r['tactix_verdict'] == 'UNSAT'
            and r['lymphosat_certificate_bytes'] > r['tactix_certificate_bytes']],
        limitations=[
            'No solver/checker execution or new independent certificate verification.',
            'Latest validity is the saved checked result; original certificates were deleted.',
            'Historical attribution is a different run, with unverified certificates and possible timing outcomes.',
            'Source/binary provenance does not establish which internal branch produced a particular model.',
            'Certificate sizes across formats and asynchronous timings do not prove causal cost differences.',
        ])
    output.mkdir(parents=True)
    csv_path = output / 'attribution.csv'
    with csv_path.open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)
    write_json(output / 'summary.json', summary)
    write_json(output / 'manifest.json', dict(schema='winner-attribution-v1',
        inputs_sha256=inputs, output_sha256={name: scorer.sha(output / name)
            for name in ('attribution.csv', 'summary.json')}))
    print(json.dumps({key: value for key,value in summary.items()
                      if key not in ('source_bindings', 'limitations')}, indent=2))


if __name__ == '__main__':
    main()
