"""Independent proof/model checks of the query and IPASIR proof fixtures.

`python3 proof_fixtures.py NAME FIXTURE ROOT [EXTRA...]` runs the spec NAME
(its CTest name) against the FIXTURE executable; it exits 77 when a pinned
checker is missing."""
from itertools import product
import hashlib
import importlib.util
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

from proof_test_support import (Fixture, audit_dsr, bundle, check_model, check_v_model, drive,
                                formula, proof_kind, satisfiable, write_cnf)

MODES = ('none', 'pbp', 'dsr')
MODES4 = MODES + ('auto',)
SCHEDULES = ('direct', 'dynamic')
SPECS = {}


def spec(function):
    SPECS[function.__name__] = function
    return function


def exact_queries(fixture, root, label, answers, *, audit=None, case_argument=True):
    """Every case of ANSWERS (case -> SAT flag per query) in every proof mode
    and both schedules: exact-query oracle, models, byte-identical proofs per
    schedule and a refutation that fails once the last clause is removed."""
    f = Fixture(root, label)
    for mode, schedule, (case, sats) in product(MODES, SCHEDULES, answers.items()):
        prefix = f.dir/f'{case}-{mode}-{schedule}'
        f.run(case, fixture, *([case] if case_argument else []), prefix, mode, schedule)
        for query, sat in enumerate(sats, 1):
            q = f.query(f'{prefix}-{query}', mode, sat, (case, query, mode), fmt=mode, audit=audit)
            if q.kind:
                f.reject_without(q.cnf, q.proof, q.kind, q.route)
    f.report(label, 'trajectories and schedule proof bytes agree')


def sats(*answers):
    return tuple(answer == 10 for answer in answers)


@spec
def derivation_history_proof(fixture, root):
    """Deleted RUP antecedents survive as checked query-certificate derivations."""
    exact_queries(fixture, root, 'derivation history', {'all': sats(10, 20, 10, 20, 10)},
                  case_argument=False)


@spec
def native_query_proof(fixture, root):
    """Exact-query certificates/models after native continuation."""
    exact_queries(fixture, root, 'native continuation', {
        case: tuple(query in (1, 4) or query == 3 and case not in ('root', 'chain')
                    for query in range(1, 6 if case == 'chain' else 4))
        for case in ('chain', 'compact', 'large', 'shrink', 'root', 'archive')})


@spec
def native_query_append_proof(fixture, root):
    """Native suffix admission: exact F+A, exhaustive answers, models and proofs."""
    exact_queries(fixture, root, 'native append', {
        'bve': sats(10, 20, 10), 'bce': sats(10, 20), 'normalize': sats(10, 20, 10),
        'growth': sats(10, 20, 20, 10, 10, 20), 'unitconflict': sats(10, 20),
        'empty': sats(10, 20), 'duplicate': sats(10, 20, 10), 'licensed': sats(10, 20, 10),
        'default': sats(10, 20, 20, 10)}, audit='trestle')


@spec
def native_repair_proof(fixture, root):
    """Native repair groups preserve exact repeated-query proofs and trajectories."""
    exact_queries(fixture, root, 'native repair',
                  {case: sats(10, 20, 20, 10) for case in ('condition', 'cce', 'binary', 'ata', 'substitute')},
                  audit='trestle')


@spec
def weakening_history_proof(fixture, root):
    """Optional native weakening capture must preserve search and proof bytes."""
    f = Fixture(root, 'weakening')
    for case, mode, capture in product(('sat', 'unsat'), MODES, ('off', 'capture')):
        prefix = f.dir/f'{case}-{mode}-{capture}'
        f.run(case, fixture, prefix, mode, capture, case)
        q = f.query(prefix, mode, case == 'sat', (case, mode), fmt=mode, audit='trestle')
        if q.kind:
            f.reject_without(q.cnf, q.proof, q.kind, q.route)
    f.report('weakening capture', 'search and proof bytes agree')


@spec
def derivation_sr_proof(fixture, root):
    """Temporary derivations must not obstruct a later witnessed continuation."""
    f = Fixture(root, 'conditioned')
    for case, mode in product(('needed', 'unneeded'), MODES):
        prefix = f.dir/f'{case}-{mode}'
        f.run(case, fixture, case, prefix, mode)
        q = f.query(prefix, mode, False, (case, mode), fmt=mode, audit='dsr')
        if q.kind and case == 'needed':
            # The globally licensed unit5 antecedent stays in the old names.
            addition = 'rup 1 x5 >= 1 ;\n' if mode == 'pbp' else '5 0\n'
            lines = q.proof.read_text().splitlines(keepends=True)
            assert lines.count(addition) == 1
            wrong = f.fresh('missing-antecedent.proof')
            wrong.write_text(''.join(line for line in lines if line != addition))
            f.reject(q.kind, q.route, q.cnf, wrong)
    f.report('conditioned derivations', 'no-proof/PBP/DSR trajectories agree')


@spec
def native_parity_proof(fixture, root):
    """Native XOR retention, later SR, exact queries and models."""
    f = Fixture(root, 'native-parity')
    for case, mode, schedule in product(('sat', 'unsat'), MODES4, SCHEDULES):
        prefix = f.dir/f'{case}-{mode}-{schedule}'
        f.run(case, fixture, case, prefix, mode, schedule)
        for query in ((2, 3, 4) if case == 'sat' else (2, 3)):
            q = f.query(f'{prefix}-{query}', mode, case == 'sat' and query != 2,
                        (case, query, mode), audit='dsr')
            if not q.kind:
                continue
            # Drop the final assumption and, for the UNSAT base, the
            # independent hard component.
            kept = q.clauses[:-1] if case == 'sat' else q.clauses[:8]+q.clauses[12:-1]
            f.reject(q.kind, q.route, f.sat_control(q.variables, kept), q.proof)
            if case == 'unsat' and query == 3:
                # A restated XOR premise the input and cube do not entail.
                text = q.proof.read_text()
                old = 'rup 1 ~x1 1 x2 1 x3 >= 1 ;\n' if q.kind == 'pbp' else '-1 2 3 0\n'
                new = old.replace('~x1', 'x1') if q.kind == 'pbp' else '1 2 3 0\n'
                assert old in text, text
                corrupted = f.fresh('corrupted-xor-source.proof')
                corrupted.write_text(text.replace(old, new, 1))
                if q.kind == 'pbp':
                    f.reject(q.kind, q.route, q.cnf, corrupted)
                else:
                    f.reject_every_lemma(q.cnf, corrupted)
    f.report('native parity', 'native trajectories and schedule bytes agree')


def corrupt_restriction_witness(text, kind):
    """Flip one witness literal of the restriction's SR lines."""
    if kind == 'pbp':
        match = re.search(r'^red 1 x(\d+) >= 1 : x\1 -> 1 x(\d+) -> 0 x(\d+) -> 1 ;$', text, re.M)
        assert match
        return text.replace(match[0], match[0].replace(f'x{match[3]} -> 1', f'x{match[3]} -> 0'), 1)
    lines = text.splitlines(keepends=True)
    candidates = []
    for i, line in enumerate(lines):
        if line.startswith('d'):
            continue
        fields = list(map(int, line.split()))
        if (len(fields) >= 8 and fields[-4] > 0 and fields[-3] == -(fields[-4]+1)
                and fields[-2] == fields[-4]+2 and fields[2] == fields[-4]):
            candidates.append((i, fields))
    assert len(candidates) >= 2
    for i, fields in candidates[:2]:
        fields[-2] *= -1
        lines[i] = ' '.join(map(str, fields))+'\n'
    return ''.join(lines)


@spec
def native_restrictions_proof(fixture, root):
    """Exact query proofs/models for restriction-dependent native continuation."""
    f = Fixture(root, 'native-restrictions')
    for case in ('sr-sat', 'sr-unsat', 'rat-sat', 'rat-unsat', 'assumption', 'append'):
        recovery = case in ('assumption', 'append')
        for mode, schedule in product(MODES4, SCHEDULES):
            prefix = f.dir/f'{case}-{mode}-{schedule}'
            f.run(case, fixture, case, prefix, mode, schedule)
            for query in (None,) if recovery else ((2,) if case.endswith('unsat') else (2, 3, 4)):
                sat = recovery or case.endswith('-sat') and query != 3
                q = f.query(prefix if recovery else f'{prefix}-{query}', mode, sat,
                            (case, query, mode), fmt='header', audit='dsr')
                if not q.kind:
                    continue
                if case.startswith('rat-') and q.kind == 'dsr':
                    assert q.route == 'dsr-cake_lpr', (case, mode, query, 'witness-free RAT replay needs SR')
                kept = ([c for c in q.clauses if set(map(abs, c)) != {9, 10}]
                        if case.endswith('unsat') else q.clauses[:-1])
                f.reject(q.kind, q.route, f.sat_control(q.variables, kept), q.proof)
                if case == 'sr-unsat':
                    corrupted = f.fresh('corrupted-restriction-witness.proof')
                    corrupted.write_text(corrupt_restriction_witness(q.proof.read_text(), q.kind))
                    if q.kind == 'pbp':
                        f.reject(q.kind, q.route, q.cnf, corrupted)
                    else:
                        f.reject_every_lemma(q.cnf, corrupted)
    f.report('native restrictions', 'native trajectories and schedule bytes agree')


@spec
def native_rollback_proof(fixture, root):
    """Original-query oracles after restriction rollback on one Kernel/H."""
    f = Fixture(root, 'native-rollback')
    for case, variant, mode, schedule in product(('assumption', 'append'), ('plain', 'collect', 'compact'),
                                                 MODES4, SCHEDULES):
        prefix = f.dir/f'{case}-{variant}-{mode}-{schedule}'
        f.run((case, variant), fixture, case, variant, prefix, mode, schedule)
        for query in range(2, 6):
            q = f.query(f'{prefix}-{query}', mode, query != 3, (case, variant, mode, query), audit='dsr')
            if q.model and query == 2:
                assert -3 in q.model
            for missing in ([1], [-1, 3]) if q.kind else ():
                kept = list(q.clauses)
                kept.remove(missing)
                f.reject(q.kind, q.route, f.sat_control(q.variables, kept), q.proof,
                         errors=('not in watch pointers',))
    f.report('native rollback', 'same Kernel/H, native trajectories and schedule bytes agree')


@spec
def query_repair_proof(fixture, root):
    """Exact-query certificates/models for same-query repair and prior results."""
    f = Fixture(root, 'query-repair')
    cases = [(name, outcome) for name in ('bve', 'condition', 'sr', 'rat', 'binding', 'alias-binding')
             for outcome in ('sat', 'unsat')]
    cases += [(f'from-{origin}', kind) for origin in ('sat', 'unsat', 'unknown')
              for kind in ('assumption', 'append')]
    for (name, outcome), mode, schedule in product(cases, MODES4, ('direct', 'registry', 'default')):
        transition = name.startswith('from-')
        prefix = f.dir/f'{name}-{outcome}-{mode}-{schedule}'
        f.run((name, outcome, schedule), fixture, name, outcome, prefix, mode, schedule, timeout=45)
        for query in (range(1, 5) if transition else (None,)):
            unknown = transition and query == 1 and name == 'from-unknown'
            sat = (query != 3 and not (query == 1 and name == 'from-unsat')
                   if transition else outcome == 'sat')
            # DefaultPolicy is a separate scheduler; direct and registry
            # schedules must emit the same bytes.
            proof_key = name, outcome, mode, query, 'default' if schedule == 'default' else 'native'
            q = f.query(f'{prefix}-{query}' if transition else prefix, mode, sat, proof_key,
                        audit='dsr', model=not unknown, timeout=45)
            if q.model and transition and query == 2:
                assert -3 in q.model
            if not q.kind:
                continue
            missing = ([5], [-5, 6, 7]) if not transition else (
                ([8], [-8, 9, 10]) if query == 1 else ([1], [-1, 3]))
            for premise in missing:
                # Keep input slots/IDs so the control isolates the premise.
                kept = list(q.clauses)
                kept[kept.index(premise)] = [1, -1]
                # An imported original clause can contradict this SAT control
                # at once: dsr-trim reports that malformed reduction as an error.
                errors = (('claims contradiction', 'No matching clause found for deletion.',
                           'not in watch pointers') if premise == missing[1] else ())
                f.reject(q.kind, q.route, f.sat_control(q.variables, kept), q.proof,
                         errors=errors, timeout=45)
    f.report('query repair', 'same-query identity, tickets, frozen prior proofs and per-schedule mode counters agree')


def tautology_control(f, cnf):
    """A satisfiable input of CNF's dimensions: every clause is `1 -1`."""
    variables, clauses = formula(cnf)
    return f.sat_control(variables, [[1, -1]] * len(clauses))


DELETION_ERROR = ('No matching clause found for deletion.',)


@spec
def query_results_proof(fixture, root):
    """Failed cores, their certificates and mode parity."""
    f = Fixture(root, 'query-results')
    names = ('binary', 'large', 'root', 'clash', 'alias', 'negative-alias',
             'learned', 'base', 'restriction', 'quota', 'disabled')
    for name, mode, schedule in product(names, MODES4, ('direct', 'registry', 'default')):
        prefix = f.dir/f'{name}-{mode}-{schedule}'
        key = name, schedule
        f.run(key, fixture, name, prefix, mode, schedule, timeout=45)
        variables, clauses = formula(Path(f'{prefix}.cnf'))
        assert not satisfiable(variables, clauses), key
        # Exact raw assumptions follow each fixture's permanent F.
        count = {'clash': 0, 'base': 2, 'learned': 4, 'large': 1, 'root': 1, 'restriction': 1}.get(name, 2)
        permanent, assumptions = clauses[:count], [c[0] for c in clauses[count:]]
        core = list(map(int, Path(f'{prefix}.core').read_text().split()))
        assert core.pop() == 0 and len(core) == len(set(core)), key
        assert set(core) <= set(assumptions), (key, core, assumptions)
        assert not satisfiable(variables, permanent+[[lit] for lit in core]), (key, core)
        f.count['UNSAT cores'] += 1
        if name == 'restriction':
            # Root SR may suggest {-1}, but that cube is SAT in F.
            assert satisfiable(variables, permanent+[[-1]])
            assert set(core) == set(assumptions)
        if mode == 'none':
            assert not Path(f'{prefix}.proof').exists()
            continue
        stems = [prefix] + ([Path(f'{prefix}-core')] if Path(f'{prefix}-core.cnf').exists() else [])
        for stem in stems:
            cnf, proof = Path(f'{stem}.cnf'), Path(f'{stem}.proof')
            kind, route = f.refute(cnf, proof, audit='dsr', timeout=45)
            f.reject(kind, route, tautology_control(f, cnf), proof, errors=DELETION_ERROR, timeout=45)
    f.report('query results', 'original assumption coordinates and per-schedule proof-mode counters agree')


@spec
def query_namespace_proof(fixture, root):
    """Exact-query namespace refresh across and within native queries."""
    f = Fixture(root, 'query-names')
    unsat_component = ({5, 6}, {-5, 6}, {5, -6}, {-5, -6})
    for case, mode in product(('cross', 'within', 'repeat', 'replay', 'fresh'),
                              ('none', 'pbp', 'dsr', 'auto', 'auto-pbp')):
        prefix = f.dir/f'{case}-{mode}'
        env = dict(os.environ, **({'TACTIX_AUTO_PROOF_SWITCH_AT': '1'} if mode == 'auto-pbp' else {}))
        f.run(case, fixture, case, prefix, mode, env=env)
        cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
        variables, clauses = formula(cnf)
        assert not satisfiable(variables, clauses)
        if mode == 'none':
            assert not proof.exists()
            continue
        if mode == 'auto-pbp' and case != 'replay':
            assert proof_kind(proof) == 'pbp'
        kind, route = f.refute(cnf, proof, fmt='header', audit='dsr')
        # Replace the independent UNSAT component by tautologies.
        kept = [[5, -5] if set(c) in unsat_component else c for c in clauses]
        f.reject(kind, route, f.sat_control(variables, kept), proof)
    f.report('query namespaces', 'native trajectories and copy work agree')


@spec
def query_projection_proof(fixture, root, default_fixture=None):
    """Query projection bridges and restrictions against the exact input."""
    f = Fixture(root, 'query-projection')
    scenarios = list(product(('bve', 'substitute', 'sr', 'ulc', 'opposite'), ('direct', 'registry')))
    if default_fixture:
        scenarios.append(('default', 'production'))
    for (scenario, schedule), mode in product(scenarios, MODES):
        prefix = f.dir/f'{scenario}-{schedule}-{mode}'
        command = ([default_fixture, prefix, mode] if scenario == 'default'
                   else [fixture, prefix, mode, scenario, schedule])
        f.run((scenario, schedule), *command)
        if mode == 'none':
            continue
        cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
        kind, route = f.refute(cnf, proof, fmt=mode, audit='trestle')
        # ULC's unrelated final assumption stays in the control.
        f.reject_without(cnf, proof, kind, route, -2 if scenario == 'ulc' else -1)
    f.report('query projection', 'proof modes agree')


@spec
def query_proof_names_proof(fixture, root):
    """Copied sources and late variable names after DSR helper allocation."""
    f = Fixture(root, 'query-proof-names')
    unlicensed = subprocess.run([fixture, f.dir/'unlicensed', 'unlicensed'],
                                capture_output=True, text=True, timeout=30)
    assert unlicensed.returncode == 1 and \
        'native clause has no checked incremental certificate source' in unlicensed.stderr, unlicensed
    for mode in ('pbp', 'dsr', 'auto', 'staged'):
        prefix = f.dir/mode
        f.run(mode, fixture, prefix, mode)
        proof = Path(f'{prefix}.proof')
        if mode == 'staged':
            lines = proof.read_text().splitlines()
            assert 'd 3 4 0' in lines and 'd 5 6 0' not in lines, lines
        kind = proof_kind(proof)
        result = f.runner.check_proof(kind, f'{prefix}.cnf', str(proof), 30,
                                      route='pbp' if kind == 'pbp' else 'dsr-trestle')
        assert result[:3] == (True, 'VERIFIED', f.runner.CHK_VERIFIED), (mode, result)
        if kind == 'dsr':
            audit(f, Path(f'{prefix}.cnf'), proof)
    for mode in ('pbp', 'dsr'):
        route = 'pbp' if mode == 'pbp' else 'dsr-cake_lpr'
        sizes = []
        for variables in (64, 128):
            prefix = f.dir/f'scale-{mode}-{variables}'
            subprocess.run([fixture, prefix, mode, str(variables)], check=True, timeout=30)
            cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
            result = f.runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
            assert result[:3] == (True, 'VERIFIED', f.runner.CHK_VERIFIED), result
            if mode == 'dsr':
                audit(f, cnf, proof)
            sizes.append(proof.stat().st_size)
            # Binding a valid stream to a satisfiable source must fail.
            wrong = prefix.with_suffix('.wrong.cnf')
            wrong.write_text(cnf.read_text().replace('-1 -2 0', '-1 2 0'))
            assert satisfiable(*formula(wrong))
            f.reject(mode, route, wrong, proof)
        assert sizes[1] < 3*sizes[0], (mode, sizes)
        print(f'query copy scaling {mode}: {sizes} bytes for 64/128 variables')
    f.report('query proof names', 'all DSR helper events and physical names match bytes')


def audit(f, cnf, proof):
    audit_dsr(f.runner, cnf, proof, f.fresh('audit'))


@spec
def native_query_sr_proof(fixture, root):
    """Witnessed continuation isolates obsolete sources; copy bridges are checked."""
    f = Fixture(root, 'native-sr', ('VERIPB', 'CAKE_PB_CNF', 'DSR_TRIM', 'DSR_COMPRESS', 'SR_CHECK'))
    from proof_oracle import canonical_checker_cnf, proof_check_status, stage_environment
    runner = f.runner
    for mode, route in (('none', None), ('pbp', 'pbp'), ('dsr', 'dsr-trestle')):
        prefix = f.dir/mode
        f.run('all', fixture, prefix, mode)
        cnf, proof = Path(f'{prefix}.cnf'), Path(f'{prefix}.proof')
        if mode == 'none':
            assert not proof.exists()
            continue
        verified = runner.check_proof(mode, str(cnf), str(proof), 30, route=route)
        assert verified[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), verified
        deletion = 'del spec 1 ~x1 1 x3 >= 1 ;\n' if mode == 'pbp' else 'd -1 3 0\n'
        text = proof.read_text()
        assert deletion not in text
        # Old names are immutable by default; an optional deletion after the
        # checked imports, before the first copy of assumption y=2, is sound.
        lines = text.splitlines(keepends=True)
        pattern = r'^red 1 x(\d+) 1 ~x2 >= 1 : x\1 -> 1 ;$' if mode == 'pbp' else r'^(\d+) -2 0$'
        candidates = [(i, re.match(pattern, line.strip())) for i, line in enumerate(lines)]
        first_copy, match = next((i, m) for i, m in candidates if m and int(m[1]) > 5)
        fresh = int(match[1])
        lines.insert(first_copy, deletion)
        retained = f.dir/f'{mode}-deleted-old-source.proof'
        retained.write_text(''.join(lines))
        checked = runner.check_proof(mode, str(cnf), str(retained), 30, route=route)
        assert checked[:3] == (True, 'VERIFIED', runner.CHK_VERIFIED), checked
        # Corrupt the reverse equivalence: its resolvent with the first
        # definition is ~y, contradicted by the query's y unit (neither RUP nor RAT).
        lines = text.splitlines(keepends=True)
        before = f'red 1 ~x{fresh} 1 x2 >= 1 : x{fresh} -> 0 ;\n' if mode == 'pbp' else f'-{fresh} 2 0\n'
        after = before.replace('1 x2', '1 ~x2') if mode == 'pbp' else f'-{fresh} -2 0\n'
        assert lines.count(before) == 1
        lines[lines.index(before)] = after
        retired = f'del spec 1 ~x{fresh} 1 x2 >= 1 ;\n' if mode == 'pbp' else f'd -{fresh} 2 0\n'
        assert lines.count(retired) == 1
        lines[lines.index(retired)] = (retired.replace('1 x2', '1 ~x2')
                                       if mode == 'pbp' else f'd -{fresh} -2 0\n')
        wrong = f.dir/f'{mode}-corrupted-copy-bridge.proof'
        wrong.write_text(''.join(lines))
        if mode == 'pbp':
            f.reject(mode, route, cnf, wrong)
            continue
        audit(f, cnf, proof)
        audit(f, cnf, retained)
        bad = f.fresh('forward-bad')
        bad.mkdir()
        canonical_checker_cnf(cnf, bad/'checker.cnf')
        checked = subprocess.run([runner.DSR_TRIM, '-f', bad/'checker.cnf', wrong, bad/'proof.lsr'],
                                 capture_output=True, text=True, timeout=30,
                                 env=stage_environment('dsr-trim'))
        assert proof_check_status('dsr-trim', checked.returncode, checked.stdout,
                                  checked.stderr) == 'rejected', checked
        assert 'No UP contradiction for RAT clause' in checked.stdout+checked.stderr, checked
    print('native SR: canonical/forward proofs tolerate retained old sources; '
          'corrupted copy bridges reject; mode trajectories agree')


def ipasir_bundles(f, fixture, timeout=45):
    """Run the bundle-writing FIXTURE once; returns its query-file prefix."""
    prefix = f.dir/'q'
    f.run('bundles', fixture, prefix, timeout=timeout)
    return prefix


def refute_query(f, stem, timeout=30):
    """STEM is UNSAT by brute force and its proof checks against STEM.cnf."""
    cnf = Path(f'{stem}.cnf')
    variables, clauses = formula(cnf)
    assert not satisfiable(variables, clauses), stem
    return f.refute(cnf, Path(f'{stem}.proof'), timeout=timeout)


@spec
def ipasir_core_proof(fixture, root):
    """IPASIR core bundles: query proofs, reduced-core proofs and SAT-source rejects."""
    f = Fixture(root, 'ipasir-core-proof')
    prefix = ipasir_bundles(f, fixture)
    for i, core in enumerate(([-1], [7, -1], [9, -9], []), 1):
        stem = f'{prefix}-{i}'
        meta = bundle(stem)
        assert list(map(int, Path(f'{stem}.core').read_text().split())) == core+[0]
        refute_query(f, stem)
        if i == 2:
            assert meta['core'] == 'full-original-cube'
            continue
        assert meta['core'] == 'licensed-subset'
        reduced = f'{stem}-core'
        kind, route = refute_query(f, reduced)
        _, clauses = formula(Path(f'{reduced}.cnf'))
        assert clauses == {1: [[1]], 3: [], 4: [[1], [-1]]}[i]+[[a] for a in core], (i, clauses)
        wrong = write_cnf(f.fresh('sat-control.cnf'), 9, [[1, -1]] * len(clauses))
        f.reject(kind, route, wrong, Path(f'{reduced}.proof'))
    f.report('IPASIR core bundles', 'bundle hashes verified')


@spec
def ipasir_cruxmiter_proof(fixture, root, name='cruxmiter'):
    """Named C strategies preserve trajectories across proof formats."""
    f = Fixture(root, f'ipasir-{name}-proof')
    for scenario, mode in product(('sat', 'unsat'), ('none', 'dsr', 'pbp', 'auto')):
        stem = f.dir/f'plain-{scenario}-{mode}'
        f.run(scenario, fixture, mode, stem, scenario, name, timeout=60)
        variables, clauses = formula(Path(f'{stem}.cnf'))
        if scenario == 'sat':
            check_model(Path(f'{stem}.model'), variables, clauses)
            f.count['SAT models'] += 1
        elif mode != 'none':
            f.refute(Path(f'{stem}.cnf'), Path(f'{stem}.proof'))
    f.report(name, 'no-proof/DSR/PBP/auto counters agree')


@spec
def ipasir_learning_proof(fixture, root):
    """Public learned exports must follow from F without assumptions or SR roots."""
    f = Fixture(root, 'ipasir-learning-proof')
    prefix = ipasir_bundles(f, fixture)
    for i, base in enumerate(([[1, 2], [1, -2]], [[1, 2], [1, -2]], [], [[1], [-1]]), 1):
        stem = f'{prefix}-{i}'
        meta = bundle(stem)
        variables, clauses = formula(Path(f'{stem}.cnf'))
        learned = []
        for line in Path(f'{stem}.learned').read_text().splitlines():
            body = list(map(int, line.split()))
            assert body.pop() == 0
            learned.append(body)
            assert not satisfiable(variables, base+[[-l] for l in body]), (i, base, body)
        assert learned == ([[1]] if i <= 2 else [] if i == 3 else [[]]), (i, learned)
        if meta['result'] == 10:
            check_v_model(Path(f'{stem}.model'), variables, clauses, complete=False)
            f.count['SAT models'] += 1
        else:
            f.refute(Path(f'{stem}.cnf'), Path(f'{stem}.proof'))
    f.report('Public learning', 'original-query bundles and F-only export oracles agree')


@spec
def ipasir_minimize_proof(fixture, root):
    """Native deletion certificates and SAT witnesses after closure."""
    spec_ = importlib.util.spec_from_file_location('trace', root/'tools/fuzz/ipasir_trace.py')
    trace = importlib.util.module_from_spec(spec_)
    spec_.loader.exec_module(trace)
    for name in ('veripb', 'cake-pb-cnf', 'drat-trim', 'cake_lpr', 'dsr-trim', 'dsr-compress', 'srcheck'):
        if not (root/'tools/checkers/bin'/name).is_file():
            sys.exit(77)
    digest = hashlib.sha256(b'tactix-permanent-input-i32le-v1\n'+struct.pack('<ii', 1, 0)).hexdigest()
    f = Fixture(root, 'ipasir-native-min')
    prefix = ipasir_bundles(f, fixture)
    for i, status in enumerate((20, 20, 10, 20, 10)*2, 1):
        stem = Path(f'{prefix}-{i}')
        meta = bundle(stem)
        assert meta['result'] == status and meta['permanent_sha256'] == digest
        clauses = trace.read_cnf(Path(f'{stem}.cnf'))
        if status == 20:
            trace.check_certificate(stem, 30)
            assert not trace.satisfiable(clauses)
        else:
            values = {int(x) for x in Path(f'{stem}.model').read_text().split() if x not in ('v', '0')}
            assert trace.satisfies(clauses, values)
    print('Native minimization: six UNSAT certificates, four SAT witnesses and exact permanent-input digests verified')


@spec
def ipasir_owned_result_proof(fixture, root):
    """Files copied from owned C handles after the originating session closes."""
    f = Fixture(root, 'ipasir-owned-proof')
    prefix = ipasir_bundles(f, fixture)
    for i, status in ((1, 20), (2, 10), (3, 20)):
        stem = f'{prefix}-{i}'
        meta = bundle(stem)
        assert meta['result'] == status
        if status == 10:
            check_v_model(Path(f'{stem}.model'), *formula(Path(f'{stem}.cnf')))
            f.count['SAT models'] += 1
        else:
            refute_query(f, stem)
        if meta['core'] == 'licensed-subset':
            refute_query(f, f'{stem}-core')
    f.report('Owned-result artifacts', 'verified after session release')


@spec
def ipasir_programmable_proof(fixture, root):
    """Original-query and reduced-core oracles for dedicated C strategies."""
    f = Fixture(root, 'ipasir-programmable-proof')
    prefix = ipasir_bundles(f, fixture, timeout=60)
    for i in range(1, 14):
        stem = f'{prefix}-{i}'
        meta = bundle(stem)
        variables, clauses = formula(Path(f'{stem}.cnf'))
        if meta['result'] == 20:
            refute_query(f, stem)
            assert Path(f'{stem}.core').read_text().split()[-1] == '0'
        else:
            assert meta['result'] == 10 and satisfiable(variables, clauses)
            check_v_model(Path(f'{stem}.model'), variables, clauses)
    refute_query(f, f'{prefix}-9-core')
    assert formula(Path(f'{prefix}-9-core.cnf'))[1] == [[1], [-1]]
    assert Path(f'{prefix}-9.core').read_text().split() == ['-1', '0']
    assert formula(Path(f'{prefix}-11.cnf'))[1] == [[-1, 2], [-1, -2], [7], [1]]
    refute_query(f, f'{prefix}-11-core')
    assert formula(Path(f'{prefix}-11-core.cnf'))[1] == [[-1, 2], [-1, -2], [1]]
    assert Path(f'{prefix}-11.core').read_text().split() == ['1', '0']
    assert formula(Path(f'{prefix}-13.cnf'))[1] == [[-1, 2], [-1, -2], [1]]
    f.report('Dedicated strategies', 'thirteen exact queries and two strict reduced cores checked')


@spec
def ipasir_query_proof(fixture, root):
    """Original-query checks for a client using only the C ABI."""
    f = Fixture(root, 'ipasir-c-client')
    queries = {1: [[1, 2], [-1]], 2: [[1, 2], [-2]], 3: [[1, 2], [7], [-1], [-2]], 4: [[1, 2]],
               5: [[1, 2], [3], [-3]], 6: [[1, 2], [3]], 7: [[1, 2], [3], []], 8: [[9], [-9]], 9: []}
    prefix = ipasir_bundles(f, fixture)
    for number, expected in queries.items():
        stem = f'{prefix}-{number}'
        cnf, proof = Path(f'{stem}.cnf'), Path(f'{stem}.proof')
        variables, clauses = formula(cnf)
        assert clauses == expected, (number, clauses)
        meta = bundle(stem)
        assert meta['query'] == (number if number < 8 else number-7) and meta['generation'] > 0
        sat = satisfiable(variables, clauses)
        assert meta['result'] == (10 if sat else 20)
        if sat:
            check_v_model(Path(f'{stem}.model'), variables, clauses)
            f.count['SAT models'] += 1
            continue
        core = list(map(int, Path(f'{stem}.core').read_text().split()))
        assert core.pop() == 0
        assert core == {3: [7, -1, -2], 5: [-3], 7: [], 8: [9, -9]}[number]
        kind, route = f.refute(cnf, proof, timeout=45)
        wrong = write_cnf(f.fresh('satisfiable-control.cnf'), variables, [[1, -1]] * len(clauses))
        f.reject(kind, route, wrong, proof, errors=DELETION_ERROR, timeout=45)
    f.report('IPASIR C client', 'nine exact input snapshots and bundle hashes verified')


@spec
def learned_clause_export_proof(fixture, root):
    """Proof-neutral learning, exact query proofs and independent F-only exports."""
    f = Fixture(root, 'learning-proof')
    polls = {}
    for scenario, mode in product(('plain', 'sr'), ('none', 'dsr', 'auto', 'pbp')):
        proofs = []
        for enabled in (0, 1):
            stem = f.dir/f'{scenario}-{mode}-{enabled}'
            run = subprocess.run([fixture, stem, mode, str(enabled), scenario],
                                 capture_output=True, text=True, timeout=45)
            assert run.returncode == 0, (scenario, mode, enabled, run.stdout, run.stderr)
            stats = dict(word.split('=') for word in run.stdout.split())
            count = stats.pop('polls')
            assert stats == f.traces.setdefault(scenario, stats), (scenario, mode, enabled, stats)
            assert count == polls.setdefault((scenario, enabled), count), (scenario, mode, enabled, count)
            variables, clauses = formula(Path(f'{stem}.cnf'))
            assert clauses == [[1, 2], [1, -2], [-1]]
            learned = [list(map(int, line.split()))[:-1]
                       for line in Path(f'{stem}.learned').read_text().splitlines()]
            assert learned == ([[1]] if enabled else [])
            for clause in learned:
                assert not satisfiable(variables, clauses[:-1]+[[-l] for l in clause])
            if mode != 'none':
                f.refute(Path(f'{stem}.cnf'), Path(f'{stem}.proof'))
                proofs.append(Path(f'{stem}.proof').read_bytes())
        assert len(set(proofs)) <= 1, (scenario, mode)
    f.report('Learning', 'F-only exports after SR, identical proof/search and termination polling')


if __name__ == '__main__':
    sys.exit(drive(SPECS))
