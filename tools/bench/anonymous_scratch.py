"""Linux scratch without directory entries, including during abnormal exit.

O_TMPFILE is mandatory: never fall back to a named file plus delayed unlink.
The kernel releases storage after the last process closes the inode, including
SIGKILL/OOM exits. Proc-fd paths let unmodified solver/checker CLIs reopen it.
"""
import os
from pathlib import Path


class AnonymousScratch:
    def __init__(self, directory="/tmp"):
        self.directory = Path(directory)
        self.handles = {}

    def __truediv__(self, name):
        if not name or Path(name).name != name:
            raise ValueError("scratch names must be single path components")
        if name not in self.handles:
            fd = os.open(self.directory, os.O_TMPFILE | os.O_RDWR | os.O_CLOEXEC, 0o600)
            if os.fstat(fd).st_nlink != 0:
                os.close(fd)
                raise RuntimeError("anonymous scratch unexpectedly has a directory entry")
            self.handles[name] = fd
        return Path(f"/proc/{os.getpid()}/fd/{self.handles[name]}")

    def close(self):
        for name in list(self.handles):
            os.close(self.handles.pop(name))

    def exists(self):
        return bool(self.handles)
