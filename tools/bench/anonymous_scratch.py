"""Linux scratch without directory entries, including during abnormal exit.

O_TMPFILE is mandatory: never fall back to a named file plus delayed unlink.
The kernel releases storage after the last process closes the inode, including
SIGKILL/OOM exits. Proc-fd paths let unmodified solver/checker CLIs reopen it.
"""
import os
import platform
import resource
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


class ScratchResourceError(RuntimeError):
    """A declared scratch resource prevented a complete checked task."""

    def __init__(self, reason, **details):
        super().__init__(reason)
        self.scratch_reason = reason
        self.scratch_details = details


class AnonymousScratch:
    def __init__(self, directory="/tmp", *, minimum_free_bytes=None,
                 maximum_file_bytes=None, maximum_live_handles=None):
        self.directory = Path(directory)
        self.handles = {}
        self.released = set()
        self.generations = {}
        self.high_water_handles = 0
        limits = (minimum_free_bytes, maximum_file_bytes, maximum_live_handles)
        if any(value is not None for value in limits):
            if any(isinstance(value, bool) or not isinstance(value, int) or value <= 0
                   for value in limits):
                raise ValueError("scratch limits must be positive integers")
            status = os.statvfs(self.directory)
            available = status.f_bavail * status.f_frsize
            details = dict(directory=str(self.directory),
                           available_bytes=available,
                           minimum_free_bytes=minimum_free_bytes,
                           maximum_file_bytes=maximum_file_bytes,
                           maximum_live_handles=maximum_live_handles)
            if available < minimum_free_bytes:
                raise ScratchResourceError("scratch_admission", **details)
            _, hard = resource.getrlimit(resource.RLIMIT_FSIZE)
            if hard != resource.RLIM_INFINITY and hard < maximum_file_bytes:
                raise ScratchResourceError(
                    "scratch_file_limit_unavailable", inherited_hard_limit=hard,
                    **details)
            try:
                resource.setrlimit(
                    resource.RLIMIT_FSIZE,
                    (maximum_file_bytes, maximum_file_bytes))
            except (OSError, ValueError) as error:
                raise ScratchResourceError(
                    "scratch_file_limit_unavailable", error=repr(error),
                    **details) from error
            if resource.getrlimit(resource.RLIMIT_FSIZE) != (
                    maximum_file_bytes, maximum_file_bytes):
                raise ScratchResourceError(
                    "scratch_file_limit_unavailable", **details)
            self.capacity = details
        else:
            self.capacity = None

    def fresh_name(self, base):
        """Return a never-reused logical name for a repeated stage artifact."""
        if not base or Path(base).name != base:
            raise ValueError("scratch names must be single path components")
        generation = self.generations.get(base, 0)
        self.generations[base] = generation + 1
        return base if generation == 0 else f"{base}.generation-{generation}"

    def __truediv__(self, name):
        if not name or Path(name).name != name:
            raise ValueError("scratch names must be single path components")
        if name in self.released:
            raise RuntimeError("released scratch name cannot be reopened")
        if name not in self.handles:
            if (self.capacity is not None and
                    len(self.handles) >= self.capacity["maximum_live_handles"]):
                raise ScratchResourceError(
                    "scratch_handle_limit", requested_name=name,
                    active_handles=len(self.handles),
                    high_water_handles=self.high_water_handles,
                    **self.capacity)
            fd = os.open(self.directory, anonymous_open_flag() | os.O_RDWR | os.O_CLOEXEC, 0o600)
            if os.fstat(fd).st_nlink != 0:
                os.close(fd)
                raise RuntimeError("anonymous scratch unexpectedly has a directory entry")
            self.handles[name] = fd
            self.high_water_handles = max(self.high_water_handles, len(self.handles))
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

    def resource_status(self):
        if self.capacity is None:
            return None
        return dict(self.capacity, active_handles=len(self.handles),
                    high_water_handles=self.high_water_handles,
                    file_size_limit_bytes=resource.getrlimit(
                        resource.RLIMIT_FSIZE)[0])

    def close(self):
        for name in list(self.handles):
            os.close(self.handles.pop(name))

    def exists(self):
        return bool(self.handles)
