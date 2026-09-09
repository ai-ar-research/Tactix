"""Hash an exact bounded certificate prefix without accepting truncation."""
import hashlib
from pathlib import Path


def hash_prefix(path, count):
    if type(count) is not int or count < 0:
        raise ValueError('prefix byte count must be a nonnegative integer')
    remaining = count
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        while remaining:
            block = stream.read(min(remaining, 1 << 20))
            if not block:
                raise ValueError('certificate shorter than required prefix')
            digest.update(block)
            remaining -= len(block)
    return dict(bytes=count, sha256=digest.hexdigest())
