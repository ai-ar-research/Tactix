"""Named per-task certificate scratch on a shared (home/NFS) filesystem.

``DirectoryScratch`` offers the logical-name interface of
``anonymous_scratch.AnonymousScratch`` (``/``, ``release``, ``fresh_name``,
``contains``, ``exists``, ``close``) for filesystems without ``O_TMPFILE``.
Every artifact is an ordinary file in one private task directory under the
campaign's declared scratch root; ``release`` unlinks it at the same checked
hand-off at which the anonymous scratch truncates and closes its descriptor,
and ``close`` removes the task directory with bounded retries for NFS
silly-renamed files.  Unlike anonymous scratch, a SIGKILLed task leaves its
directory behind; the campaign aggregate reports such leftovers.
"""
import errno
import os
from pathlib import Path
import shutil
import time

SCHEMA = "tactix-home-directory-scratch-v1"
CHILD_TMPDIR = "tmp"
CLEANUP_ATTEMPTS = 20
CLEANUP_INITIAL_SECONDS = 0.05
CLEANUP_MAX_SECONDS = 2.0


def remove_tree(path, attempts=CLEANUP_ATTEMPTS, initial_delay=CLEANUP_INITIAL_SECONDS,
                max_delay=CLEANUP_MAX_SECONDS):
    """Remove ``path``; retry transient NFS EBUSY/ENOTEMPTY. Returns retries."""
    path = Path(path)
    delay, retries = initial_delay, 0
    for attempt in range(attempts):
        if not path.exists():
            return retries
        try:
            shutil.rmtree(path)
            return retries
        except FileNotFoundError:
            return retries
        except OSError as error:
            if (error.errno not in (errno.EBUSY, errno.ENOTEMPTY) or
                    attempt + 1 == attempts):
                raise
            retries += 1
            time.sleep(delay)
            delay = min(max_delay, delay * 2)
    return retries


class DirectoryScratch:
    def __init__(self, root, name):
        root = Path(root)
        if not root.is_absolute():
            raise ValueError("directory scratch root must be absolute")
        if not name or Path(name).name != name or name in (".", ".."):
            raise ValueError("task scratch name must be one path component")
        root.mkdir(parents=True, exist_ok=True)
        self.directory = root / name
        # Exclusive: a leftover directory from a dead task is never reused.
        self.directory.mkdir(mode=0o700)
        self.tmpdir = self.directory / CHILD_TMPDIR
        self.tmpdir.mkdir(mode=0o700)
        self.live = set()
        self.released = set()
        self.generations = {}
        self.released_bytes = {}
        self.cleanup_retries = None
        self.closed = False

    def fresh_name(self, base):
        if not base or Path(base).name != base:
            raise ValueError("scratch names must be single path components")
        generation = self.generations.get(base, 0)
        self.generations[base] = generation + 1
        return base if generation == 0 else f"{base}.generation-{generation}"

    def __truediv__(self, name):
        if (not name or Path(name).name != name or
                name in (CHILD_TMPDIR, ".", "..")):
            raise ValueError("scratch names must be single path components")
        if name in self.released:
            raise RuntimeError("released scratch name cannot be reopened")
        self.live.add(name)
        return self.directory / name

    def __fspath__(self):
        return str(self.directory)

    def release(self, name):
        """Delete one finished artifact now; idempotent like AnonymousScratch."""
        if not isinstance(name, str) or not name or Path(name).name != name:
            raise TypeError("release requires one logical scratch name")
        if name in self.released or name not in self.live:
            return False
        self.live.discard(name)
        self.released.add(name)
        path = self.directory / name
        try:
            size = path.stat().st_size
        except FileNotFoundError:
            size = None
        path.unlink(missing_ok=True)
        self.released_bytes[name] = size
        return True

    def contains(self, name):
        return name in self.live

    def accounted_bytes(self):
        total = 0
        for name in list(self.live):
            try:
                total += (self.directory / name).stat().st_blocks * 512
            except OSError:
                pass
        return total

    def exists(self):
        """Live artifacts before close; any leftover directory after close."""
        return bool(self.live) or (self.closed and self.directory.exists())

    def close(self):
        self.live.clear()
        self.closed = True
        self.cleanup_retries = remove_tree(self.directory)

    def resource_status(self):
        return {"schema": SCHEMA, "directory": str(self.directory),
                "released_bytes": dict(sorted(self.released_bytes.items())),
                "cleanup_retries": self.cleanup_retries,
                "removed": not self.directory.exists()}
