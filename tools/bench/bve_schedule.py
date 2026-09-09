"""Strict reader for the complete, stopped BVE schedule observer (version 1)."""
import re


def parse(lines):
    lines = iter(lines)
    if next(lines, '').rstrip('\n') != '# bve-schedule-observer\t1':
        raise ValueError('unsupported BVE schedule header')
    row = next(lines, '').rstrip('\n').split('\t')
    if len(row) != 2 or row[0] not in ('tactix', 'kissat'):
        raise ValueError('missing schedule summary')

    def fields(items):
        result = {}
        for item in items:
            pair = item.split('=')
            if len(pair) != 2 or not all(pair) or pair[0] in result:
                raise ValueError('invalid or duplicate schedule field')
            result[pair[0]] = pair[1]
        return result

    header = fields(row[1].split('|'))
    if not header.get('scheduled', '').isdigit():
        raise ValueError('missing schedule size')
    members = []
    seen = set()
    for line in lines:
        row = line.rstrip('\n').split('\t')
        if row[0] != 'member':
            raise ValueError('expected complete member row')
        member = fields(row[1:])
        if set(member) != {'external', 'position', 'positive', 'negative', 'score'}:
            raise ValueError('invalid member schema')
        if any(not member[key].isdigit() for key in ('external', 'position', 'positive', 'negative')):
            raise ValueError('invalid member integer')
        if not re.fullmatch('[0-9a-f]{16}', member['score']):
            raise ValueError('invalid exact score bits')
        external = int(member['external'])
        if not external or external in seen or int(member['position']) != len(members):
            raise ValueError('duplicate member or noncontiguous position')
        seen.add(external)
        members.append(member)
    if len(members) != int(header['scheduled']):
        raise ValueError('incomplete schedule')
    return header, members
