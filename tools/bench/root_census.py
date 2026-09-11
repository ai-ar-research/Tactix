"""Strict reader for isolated root/storage census; bytes are not RSS savings."""
FIELDS = set('seq conflicts decisions root_events max_rss_kib vars fixed fixed_active retired unassigned selected_size_bytes selected_capacity_bytes watch_words watch_capacity watch_holes arena_words arena_capacity reconstruction_words reconstruction_capacity dense_words dense_capacity dense_holes dense_headers dense_pending identity_capacity heuristic_capacity input_capacity heap_arena heap_live heap_free mmap_bytes'.split())
STAGES = {'phase-entry','phase-exit','bve-before','bve-after','fixture','disconnect'}


def parse(text):
    rows = []
    for line in text.splitlines():
        cells = line.split('\t')
        if cells.pop(0) != 'census': raise ValueError('wrong record prefix')
        values = {}
        for cell in cells:
            key, separator, value = cell.partition('=')
            if not separator or key in values: raise ValueError('missing/duplicate field')
            values[key] = value
        if set(values) != FIELDS | {'stage'} or values['stage'] not in STAGES:
            raise ValueError('wrong census schema')
        if any(not values[key].isascii() or not values[key].isdecimal() for key in FIELDS):
            raise ValueError('nonnegative integer required')
        row = {key:int(values[key]) for key in FIELDS}
        row['stage'] = values['stage']
        if row['seq'] != len(rows)+1: raise ValueError('lost/repeated census row')
        if rows and (row['conflicts'] < rows[-1]['conflicts'] or row['decisions'] < rows[-1]['decisions']):
            raise ValueError('work regressed')
        if not (row['fixed_active'] <= row['fixed'] <= row['vars'] and
                row['retired']+row['fixed_active']+row['unassigned'] <= row['vars']):
            raise ValueError('inconsistent populations')
        for used, capacity in (('selected_size_bytes','selected_capacity_bytes'),
                               ('watch_words','watch_capacity'),('arena_words','arena_capacity'),
                               ('reconstruction_words','reconstruction_capacity')):
            if row[used] > row[capacity]: raise ValueError('size exceeds capacity')
        if row['watch_holes'] > row['watch_words'] or row['dense_holes'] > row['dense_words']:
            raise ValueError('holes exceed storage')
        if 4*row['dense_words'] > row['dense_capacity']:
            raise ValueError('dense capacity unit mismatch')
        if row['heap_live']+row['heap_free'] != row['heap_arena']:
            raise ValueError('allocator conservation failed')
        rows.append(row)
    if not rows: raise ValueError('empty census')
    return rows
