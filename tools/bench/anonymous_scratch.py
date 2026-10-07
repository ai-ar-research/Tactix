"""Linux scratch without directory entries, including during abnormal exit.

O_TMPFILE is mandatory: never fall back to a named file plus delayed unlink.
The kernel releases storage after the last process closes the inode, including
SIGKILL/OOM exits. Proc-fd paths let unmodified solver/checker CLIs reopen it.
"""
import os
import platform
import sys
from pathlib import Path


def anonymous_open_flag():
    flag = getattr(os, "O_TMPFILE", None)
    if flag is not None:
        return flag
    # Python built against older Linux headers can omit the name even when
    # the running kernel supports it. Use the Linux UAPI value only on the
    # architectures whose ABI defines this bit; the kernel still checks support.
    if sys.platform == "linux" and platform.machine().lower() in ("x86_64", "amd64", "aarch64"):
        return 0o20000000 | os.O_DIRECTORY
    raise OSError("O_TMPFILE is unavailable for this platform")


class AnonymousScratch:
    def __init__(self, directory="/tmp"):
        self.directory = Path(directory)
        self.handles = {}
        self.released = set()

    def __truediv__(self, name):
        if not name or Path(name).name != name:
            raise ValueError("scratch names must be single path components")
        if name in self.released:
            raise RuntimeError("released scratch name cannot be reopened")
        if name not in self.handles:
            fd = os.open(self.directory, anonymous_open_flag() | os.O_RDWR | os.O_CLOEXEC, 0o600)
            if os.fstat(fd).st_nlink != 0:
                os.close(fd)
                raise RuntimeError("anonymous scratch unexpectedly has a directory entry")
            self.handles[name] = fd
        return Path(f"/proc/{os.getpid()}/fd/{self.handles[name]}")

    def release(self, name):
        """Return one finished artifact's blocks before the task exits.

        Callers pass the logical name, never a proc-fd Path. File-descriptor
        numbers may be reused, so accepting a stale proc path could otherwise
        discard a newer artifact. Released names are tombstoned and cannot be
        reopened within this task.
        Truncation also frees the inode if a terminated helper leaked a copied
        descriptor; closing removes this owner's final reference in the normal
        case. Releasing an already-released name is an idempotent no-op.
        """
        if not isinstance(name, str) or not name or Path(name).name != name:
            raise TypeError("release requires one logical scratch name")
        if name in self.released:
            return False
        fd = self.handles.pop(name, None)
        if fd is None:
            return False
        truncate_error = None
        close_error = None
        try:
            os.ftruncate(fd, 0)
        except BaseException as error:
            truncate_error = error
        try:
            os.close(fd)
        except BaseException as error:
            close_error = error
        finally:
            self.released.add(name)
        if truncate_error is not None:
            raise truncate_error
        if close_error is not None:
            raise close_error
        return True

    def contains(self, name):
        return name in self.handles

    def close(self):
        for name in list(self.handles):
            os.close(self.handles.pop(name))

    def exists(self):
        return bool(self.handles)
