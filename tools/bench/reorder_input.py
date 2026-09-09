"""Reconstruct selected Reorder weights from complete incident root snapshots.

Large rows use the validated (key, physical_pose, metadata, literal_values)
schema of compare-rank32-formula-5785. This is an offline observer, not a solver.
An active-set member proves unassigned only at a separately verified root
boundary. Absence from that set proves nothing about a retired variable's value.
"""
import math


def root_boundary(content, *, event, conflict):
    columns = ('solver seq event conflicts decisions propagations level mode unassigned trail '
               'restarts reductions switched rephased stamp').split()
    lines = content.splitlines()
    if not content.endswith('\n') or lines[:2] != ['# tactix-macro-boundary\t1', '# ' + '\t'.join(columns)]:
        raise ValueError('invalid macro boundary header')
    selected = []
    for line in lines[2:]:
        fields = line.split('\t')
        if len(fields) != len(columns) or fields[0] != 'tactix':
            raise ValueError('invalid macro boundary row')
        if fields[2] == event and int(fields[3]) == conflict:
            selected.append(dict(zip(columns, fields)))
    if len(selected) != 1 or selected[0]['level'] != '0':
        raise ValueError('requires unique selected root boundary')
    return selected[0]


def read_active(content, *, event, conflict):
    if not content.endswith('\n'):
        raise ValueError('unterminated active set')
    lines = content.splitlines()
    if len(lines) < 4 or lines[:2] != ['# tactix-macro-active-set\t1',
                                    '# solver\tevent\tconflicts\tactive']:
        raise ValueError('invalid active set header')
    boundary = lines[2].split('\t')
    if (len(boundary) != 4 or boundary[:3] != ['# tactix', event, str(conflict)]
            or lines[3] != '# external'):
        raise ValueError('wrong active set boundary')
    count = int(boundary[-1])
    members = [int(line) for line in lines[4:]]
    if count < 0 or len(members) != count or len(set(members)) != count or any(v <= 0 for v in members):
        raise ValueError('invalid active membership/count')
    return set(members)


def reconstruct(external, formula, binaries, active, *, level):
    if external <= 0 or level != 0 or external not in active:
        raise ValueError('requires selected active variable at a verified root boundary')
    values = {variable: 0 for variable in active}
    for (_, literals), pose, _, observed in formula:
        if tuple(sorted(pose)) != literals or tuple(lit for lit, _ in observed) != literals:
            raise ValueError('inconsistent large-clause framing')
        for lit, value in observed:
            if not lit or value not in (-1, 0, 1):
                raise ValueError('invalid literal value')
            variable, positive = abs(lit), value if lit > 0 else -value
            if variable in values and values[variable] != positive:
                raise ValueError('inconsistent represented value or active state')
            values[variable] = positive
    weights = {external: 0.0, -external: 0.0}
    large = []
    for index, ((redundant, literals), pose, _, observed) in enumerate(formula):
        if redundant not in (0, 1) or len(literals) < 3 or not any(abs(lit) == external for lit in literals):
            raise ValueError('invalid incident large clause')
        contribution, opened, reason = 0.0, 0, 'redundant'
        if not redundant:
            reason = 'counted'
            for lit in pose:
                value = values[abs(lit)] * (1 if lit > 0 else -1)
                if value > 0:
                    reason = 'satisfied'
                    break
                if value == 0:
                    opened += 1
                    if opened == 100:
                        break
            if reason == 'counted':
                contribution = math.ldexp(1.0, -(max(2, opened) - 2))
            for lit in literals:
                if lit in weights:
                    weights[lit] += contribution
        large.append(dict(index=index, redundant=redundant, literals=literals,
                          open_seen=opened, reason=reason, weight=contribution))
    large_weights = dict(weights)
    binary = []
    for pair in binaries:
        if len(pair) != 2 or not all(pair) or pair[0] >= pair[1] or not any(abs(lit) == external for lit in pair):
            raise ValueError('invalid incident binary')
        if any(abs(lit) not in values for lit in pair):
            raise ValueError('unknown binary endpoint value; do not assume free')
        counted = all(values[abs(lit)] == 0 for lit in pair)
        if counted:
            for lit in pair:
                if lit in weights:
                    weights[lit] += 1.0
        binary.append(dict(literals=pair, counted=counted,
                           values=[values[abs(lit)] * (1 if lit > 0 else -1) for lit in pair]))
    pos, neg = weights[external], weights[-external]
    return dict(positive=pos, negative=neg, combined=max(pos, neg) + 2.0 * min(pos, neg),
                positive_large=large_weights[external], negative_large=large_weights[-external],
                large=large, binaries=binary, represented_variables=len(values))
