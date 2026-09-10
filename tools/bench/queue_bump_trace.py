"""Parse pre-move focused queue observations without normalizing away state."""

FIELDS = ('conflicts decisions stable variable stamp previous next search '
          'search_stamp queue_stamp value').split()


def read_bumps(content, *, window, cap):
    if len(window) != 2 or not 0 <= window[0] <= window[1] or cap <= 0:
        raise ValueError('invalid scope')
    if not content or not content.endswith('\n'):
        raise ValueError('empty or incomplete bump trace')
    result = []
    for line in content.splitlines():
        cells = line.split('\t')
        if len(cells) != len(FIELDS):
            raise ValueError('invalid bump width')
        try:
            row = dict(zip(FIELDS, map(int, cells)))
        except ValueError as error:
            raise ValueError('invalid bump integer') from error
        if any(row[k] < 0 for k in FIELDS[:-1]):
            raise ValueError('negative counter or identity')
        if row['stable'] != 0 or row['variable'] == 0 or row['value'] not in (-1, 0, 1):
            raise ValueError('invalid focused bump identity')
        if not window[0] <= row['conflicts'] <= window[1]:
            raise ValueError('bump outside window')
        if result and any(row[k] < result[-1][k] for k in ('conflicts', 'decisions')):
            raise ValueError('nonmonotone bump counters')
        result.append(row)
    if len(result) >= cap:
        raise ValueError('capped bump trace')
    return result
