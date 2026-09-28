#!/usr/bin/env python3
"""Score the pinned no-MAB hybrid gate reruns, preserving WV2 timeouts."""
import csv
import hashlib
import json
import math
from pathlib import Path

from fixed_runtime import score as fixed_score

ROOT = Path(__file__).resolve().parents[2]
PREFIX = '20260928-am549-no-mab-hybrid-'
SETS = {'rank1-50': 50, 'b28c5': 33, 'wv100': 100, 'wv2-89': 89}
ROUTES = {
    'veripb-cake-pb': ('pbp', 'pbp', ['veripb', 'cake-pb-cnf']),
    'dsr-trim-dsr-compress-trestle': ('dsr-trestle', 'dsr',
                                     ['dsr-trim', 'dsr-compress', 'srcheck']),
    'dsr-as-drat-drat-trim-cake_lpr': ('dsr-cake_lpr', 'drat',
                                       ['drat-trim', 'cake_lpr']),
}


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def score_set(name, size):
    campaign = ROOT / 'benchmarks/runs' / (PREFIX + name)
    manifest = json.loads((campaign / 'manifest.json').read_text())
    old = ROOT / manifest['predecessor']['path']
    require(sha(old) == manifest['predecessor']['sha256'], name + ': old manifest changed')
    for rel, digest in manifest['pin_sha256'].items():
        require(sha(campaign / rel) == digest, name + ': pin changed: ' + rel)
    with (campaign / 'matrix.tsv').open(newline='') as source:
        rows = list(csv.DictReader(source, delimiter='\t'))
    require(len(rows) == size, name + ': matrix size')
    predecessor = campaign.parent / ('20260927-am519-no-mab-' + name)
    require((campaign / 'matrix.tsv').read_bytes() == (predecessor / 'matrix.tsv').read_bytes(),
            name + ': frozen instance list changed')
    result_paths = sorted((campaign / 'results').glob('*.json'))
    require([p.name for p in result_paths] == [f'{i:03}.json' for i in range(size)],
            name + ': missing final result')
    require(len(list((campaign / 'provisional').glob('*.json'))) == size,
            name + ': missing provisional result')
    route_counts = {route: 0 for route in ROUTES}
    sat = unsat = timeouts = checker_timeouts = 0
    seconds = []
    par2 = []
    bad = []
    max_rss = 0
    for i, (row, path) in enumerate(zip(rows, result_paths)):
        r = json.loads(path.read_text())
        require(all(str(r[key]) == row[key] for key in ('task', 'rank', 'arm', 'tier', 'instance', 'expected')),
                name + f': result identity {i}')
        require(r['binary_sha256'] == manifest['arms'][row['arm']]['sha256'],
                name + f': binary identity {i}')
        require(r['artifact_deleted'] and r['derived_artifacts_deleted'] and not r['cleanup_pending'],
                name + f': cleanup {i}')
        solve = r['solve']
        require(0 <= solve['wall_seconds'] <= manifest['resource_contract']['solve_timeout'] + 2,
                name + f': solve timing {i}')
        if solve['peak_rss_kb'] is not None:
            max_rss = max(max_rss, solve['peak_rss_kb'])
            require(solve['peak_rss_kb'] <= 32768 * 1024, name + f': memory {i}')
        if solve['timed_out'] and r['verdict'] == 'UNKNOWN':
            require(name in ('wv100', 'wv2-89') and
                    r['proof_check'] is None and r['model_check'] is None,
                    name + f': unexpected timeout {i}')
            timeouts += 1
            par2.append(2 * manifest['resource_contract']['solve_timeout'])
            continue
        par2.append(solve['wall_seconds'])
        seconds.append(solve['wall_seconds'])
        require(r['expected_match'] and r['verdict'] == row['expected'],
                name + f': wrong solver verdict {i}')
        require(solve['exit_code'] == {'SAT': 10, 'UNSAT': 20}[r['verdict']] and not solve['timed_out'],
                name + f': solve status {i}')
        if (name in ('wv100', 'wv2-89') and r['verdict'] == 'UNSAT' and
                r['proof_check'] is not None and r['proof_check']['timed_out']):
            check = r['proof_check']
            require(not r['valid'] and r['model_check'] is None and
                    check['result'] == 'not-verified' and check['pipeline'] in ROUTES,
                    name + f': invalid checker-timeout record {i}')
            reported, content, stages = ROUTES[check['pipeline']]
            observed = check['stages']
            require(check['solver_reported_route'] == reported and
                    check['certificate_content_route'] == content and observed and
                    [stage['name'] for stage in observed] == stages[:len(observed)] and
                    all(stage['result'] == 'verified' for stage in observed[:-1]) and
                    observed[-1]['timed_out'],
                    name + f': checker timeout was not the sole failed stage {i}')
            checker_timeouts += 1
            continue
        require(r['valid'], name + f': rejected certificate {i}')
        if r['verdict'] == 'SAT':
            check = r['model_check']
            require(check is not None and check['result'] == 'verified' and not check['timed_out'] and
                    r['proof_check'] is None, name + f': model {i}')
            sat += 1
        else:
            check = r['proof_check']
            require(check is not None and check['result'] == 'verified' and not check['timed_out'] and
                    r['model_check'] is None, name + f': proof {i}')
            route = check['pipeline']
            require(route in ROUTES, name + f': proof route {i}')
            reported, content, stages = ROUTES[route]
            require(check['solver_reported_route'] == reported and
                    check['certificate_content_route'] == content and
                    [stage['name'] for stage in check['stages']] == stages and
                    all(stage['result'] == 'verified' for stage in check['stages']),
                    name + f': independent proof stages {i}')
            route_counts[route] += 1
            unsat += 1
    require(sat + unsat + timeouts + checker_timeouts == size,
            name + ': count mismatch')
    output = dict(campaign=str(campaign.relative_to(ROOT)),
                  manifest_sha256=sha(campaign / 'manifest.json'),
                  complete=True, rows=size, checked=sat + unsat,
                  sat_models=sat, unsat_proofs=unsat, solver_timeouts=timeouts,
                  checker_timeouts=checker_timeouts,
                  routes=route_counts, solver_seconds=math.fsum(seconds),
                  par2_seconds=math.fsum(par2), max_solver_seconds=max(seconds),
                  max_peak_rss_kb=max_rss, failures=bad)
    if name in ('rank1-50', 'b28c5'):
        target = {'rank1-50': 8521.52175, 'b28c5': 999.87615}[name]
        reference = fixed_score(campaign, runtime_target_seconds=target)
        require(reference['complete'] and reference['checked_clean'] == size,
                name + ': fixed-reference scorer disagrees')
        output['fixed_reference'] = reference
        output['within_five_percent'] = output['solver_seconds'] <= target
        require(output['within_five_percent'], name + ': aggregate runtime exceeds leeway')
    return output


def main():
    results = {name: score_set(name, size) for name, size in SETS.items()}
    print(json.dumps(dict(schema='tactix-am549-hybrid-milestone-score-v1',
                          sets=results,
                          all_decided_certificates_checked=all(
                              result['checker_timeouts'] == 0
                              for result in results.values()),
                          wv2_solve_coverage=results['wv2-89']['checked']), indent=2))


if __name__ == '__main__':
    main()
