"""Linux scratch without directory entries, including during abnormal exit.

O_TMPFILE is mandatory: never fall back to a named file plus delayed unlink.
The kernel releases storage after the last process closes the inode, including
SIGKILL/OOM exits. Proc-fd paths let unmodified solver/checker CLIs reopen it.
"""
import os
import platform
import resource
import errno
import fcntl
import stat
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
                 maximum_file_bytes=None, maximum_live_handles=None,
                 cooperative_lock_directory=None,
                 cooperative_lock_name=None):
        self.directory = Path(directory)
        self.handles = {}
        self.released = set()
        self.generations = {}
        self.high_water_handles = 0
        self.cooperative_lock_fd = None
        self.cooperative_lock = None
        limits = (minimum_free_bytes, maximum_file_bytes, maximum_live_handles)
        if any(value is not None for value in limits):
            if any(isinstance(value, bool) or not isinstance(value, int) or value <= 0
                   for value in limits):
                raise ValueError("scratch limits must be positive integers")
            try:
                if ((cooperative_lock_directory is None) !=
                        (cooperative_lock_name is None)):
                    raise ValueError(
                        "cooperative lock directory and name must be provided together")
                if cooperative_lock_directory is not None:
                    self._acquire_cooperative_lock(
                        cooperative_lock_directory, cooperative_lock_name)
                status = os.statvfs(self.directory)
                available = status.f_bavail * status.f_frsize
                details = dict(directory=str(self.directory),
                               available_bytes=available,
                               minimum_free_bytes=minimum_free_bytes,
                               maximum_file_bytes=maximum_file_bytes,
                               maximum_live_handles=maximum_live_handles)
                if self.cooperative_lock is not None:
                    details["cooperative_lock"] = dict(self.cooperative_lock)
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
            except BaseException as error:
                self._release_cooperative_lock()
                raise
        else:
            if (cooperative_lock_directory is not None or
                    cooperative_lock_name is not None):
                raise ValueError("cooperative locking requires scratch limits")
            self.capacity = None

    def _acquire_cooperative_lock(self, directory, name):
        lock_directory = Path(directory)
        if (lock_directory.parent != self.directory or not name or
                Path(name).name != name):
            raise ValueError("cooperative lock must be one private scratch child")
        try:
            os.mkdir(lock_directory, 0o700)
        except FileExistsError:
            pass
        directory_stat = os.lstat(lock_directory)
        if (not stat.S_ISDIR(directory_stat.st_mode) or
                directory_stat.st_uid != os.getuid() or
                stat.S_IMODE(directory_stat.st_mode) != 0o700):
            raise OSError(errno.EPERM, "cooperative lock directory is not private")
        directory_fd = os.open(
            lock_directory,
            os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW)
        lock_fd = None
        try:
            lock_fd = os.open(
                name, os.O_RDWR | os.O_CREAT | os.O_CLOEXEC | os.O_NOFOLLOW,
                0o600, dir_fd=directory_fd)
            lock_stat = os.fstat(lock_fd)
            if (not stat.S_ISREG(lock_stat.st_mode) or
                    lock_stat.st_uid != os.getuid() or
                    stat.S_IMODE(lock_stat.st_mode) != 0o600 or
                    lock_stat.st_nlink != 1):
                raise OSError(errno.EPERM, "cooperative lock file is not private")
            try:
                fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError as error:
                if error.errno not in (errno.EACCES, errno.EAGAIN):
                    raise
                raise ScratchResourceError(
                    "scratch_node_busy", directory=str(self.directory),
                    cooperative_lock_directory=str(lock_directory),
                    cooperative_lock_name=name) from error
            self.cooperative_lock_fd = lock_fd
            self.cooperative_lock = {
                "directory": str(lock_directory), "name": name,
                "device": lock_stat.st_dev, "inode": lock_stat.st_ino,
            }
            lock_fd = None
        finally:
            if lock_fd is not None:
                os.close(lock_fd)
            os.close(directory_fd)

    def _release_cooperative_lock(self):
        if self.cooperative_lock_fd is not None:
            os.close(self.cooperative_lock_fd)
            self.cooperative_lock_fd = None

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

    def accounted_bytes(self):
        """Allocated bytes of this owner's live anonymous files.

        Safe to call from a monitor thread: it inspects a snapshot of the
        descriptors and ignores one that the owner closed concurrently.
        """
        total = 0
        try:
            descriptors = list(self.handles.values())
        except RuntimeError:
            return None
        for fd in descriptors:
            try:
                total += os.fstat(fd).st_blocks * 512
            except OSError:
                pass
        return total

    def resource_status(self):
        if self.capacity is None:
            return None
        return dict(self.capacity, active_handles=len(self.handles),
                    high_water_handles=self.high_water_handles,
                    file_size_limit_bytes=resource.getrlimit(
                        resource.RLIMIT_FSIZE)[0])

    def close(self):
        first_error = None
        try:
            for name in list(self.handles):
                try:
                    os.close(self.handles.pop(name))
                except BaseException as error:
                    if first_error is None:
                        first_error = error
        finally:
            try:
                self._release_cooperative_lock()
            except BaseException as error:
                if first_error is None:
                    first_error = error
        if first_error is not None:
            raise first_error

    def exists(self):
        return bool(self.handles)


def _private_child_directory_fd(scratch_directory, directory):
    """Open a private same-user child of the scratch root without following links."""
    child = Path(directory)
    if child.parent != Path(scratch_directory):
        raise ValueError("ledger must be one private scratch child")
    try:
        os.mkdir(child, 0o700)
    except FileExistsError:
        pass
    child_stat = os.lstat(child)
    if (not stat.S_ISDIR(child_stat.st_mode) or
            child_stat.st_uid != os.getuid() or
            stat.S_IMODE(child_stat.st_mode) != 0o700):
        raise OSError(errno.EPERM, "ledger directory is not private")
    return os.open(child, os.O_RDONLY | os.O_DIRECTORY | os.O_CLOEXEC | os.O_NOFOLLOW)


def _open_private_slot(directory_fd, name, flags):
    fd = os.open(name, flags | os.O_CLOEXEC | os.O_NOFOLLOW, 0o600,
                 dir_fd=directory_fd)
    try:
        slot_stat = os.fstat(fd)
        if (not stat.S_ISREG(slot_stat.st_mode) or
                slot_stat.st_uid != os.getuid() or
                stat.S_IMODE(slot_stat.st_mode) != 0o600 or
                slot_stat.st_nlink != 1):
            raise OSError(errno.EPERM, "ledger slot is not private")
    except BaseException:
        os.close(fd)
        raise
    return fd


class NodeScratchLedger:
    """Node-local participants of the shared-floor scratch profile.

    A fixed set of stable slot files lives in the private per-user directory.
    A participant owns one slot through an exclusive ``flock`` held until it
    closes, so the kernel releases a dead owner's slot (including SIGKILL/OOM);
    no record is ever trusted without its live lock. The owner rewrites one
    fixed-width record with its accounted anonymous bytes and state. Slot files
    hold only that bounded metadata and never certificate bytes.
    """
    RECORD_BYTES = 192

    def __init__(self, scratch_directory, ledger_directory, slot_prefix, slots,
                 identity=None):
        if (isinstance(slots, bool) or not isinstance(slots, int) or
                not 1 <= slots <= 256):
            raise ValueError("ledger slots must be in 1..256")
        if not slot_prefix or Path(slot_prefix).name != slot_prefix:
            raise ValueError("ledger slot prefix must be one path component")
        self.directory = Path(ledger_directory)
        self.slot_prefix = slot_prefix
        self.slots = slots
        self.identity = str(identity if identity is not None else os.getpid())
        self.slot = None
        self.slot_fd = None
        self.state = "running"
        directory_fd = _private_child_directory_fd(scratch_directory, ledger_directory)
        try:
            for index in range(slots):
                fd = _open_private_slot(directory_fd, self.slot_name(index),
                                        os.O_RDWR | os.O_CREAT)
                try:
                    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                except OSError as error:
                    os.close(fd)
                    if error.errno not in (errno.EACCES, errno.EAGAIN):
                        raise
                    continue
                self.slot, self.slot_fd = index, fd
                break
        finally:
            os.close(directory_fd)
        if self.slot_fd is None:
            raise ScratchResourceError(
                "scratch_admission", stage="ledger", ledger_full=True,
                ledger_directory=str(self.directory), ledger_slots=slots)
        self.publish(0)

    def slot_name(self, index):
        return f"{self.slot_prefix}{index:03d}"

    def publish(self, used_bytes, state=None):
        if self.slot_fd is None:
            return
        if state is not None:
            self.state = state
        record = (f"{int(used_bytes or 0)} {self.state} {self.identity}"
                  .encode()[:self.RECORD_BYTES - 1])
        os.pwrite(self.slot_fd, record.ljust(self.RECORD_BYTES - 1) + b"\n", 0)

    def participants(self):
        """Live participant records; a slot is live only while its lock is held."""
        live = []
        directory_fd = _private_child_directory_fd(self.directory.parent, self.directory)
        try:
            for index in range(self.slots):
                if index == self.slot:
                    record = os.pread(self.slot_fd, self.RECORD_BYTES, 0)
                else:
                    try:
                        fd = _open_private_slot(directory_fd, self.slot_name(index),
                                                os.O_RDONLY)
                    except FileNotFoundError:
                        continue
                    try:
                        try:
                            fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
                        except OSError as error:
                            if error.errno not in (errno.EACCES, errno.EAGAIN):
                                raise
                            record = os.pread(fd, self.RECORD_BYTES, 0)
                        else:
                            continue  # free slot: its last record is stale
                    finally:
                        os.close(fd)
                fields = record.decode(errors="replace").split()
                try:
                    used = int(fields[0])
                except (IndexError, ValueError):
                    used = 0
                live.append(dict(slot=index, used_bytes=used,
                                 state=fields[1] if len(fields) > 1 else "unknown",
                                 identity=fields[2] if len(fields) > 2 else None))
        finally:
            os.close(directory_fd)
        return live

    def close(self):
        if self.slot_fd is not None:
            fd, self.slot_fd = self.slot_fd, None
            os.close(fd)


def admit_shared_scratch(directory, *, admission_free_bytes, maximum_file_bytes,
                         maximum_live_handles, ledger_directory, ledger_slot_prefix,
                         ledger_slots, admission_wait_seconds,
                         admission_poll_seconds, identity=None,
                         clock=None, sleep=None):
    """Join the node ledger, then admit once free space reaches the threshold.

    Unlike the cooperative mutex, co-located participants are admitted
    together; a refusal only means the node lacks the admission headroom (or a
    ledger slot) for the whole bounded wait. Returns ``(scratch, ledger,
    admission)``; nothing anonymous is opened before admission.
    """
    import time
    clock = clock or time.monotonic
    sleep = sleep or time.sleep
    started = clock()
    attempts = 0
    while True:
        attempts += 1
        ledger = None
        try:
            ledger = NodeScratchLedger(directory, ledger_directory,
                                       ledger_slot_prefix, ledger_slots, identity)
            scratch = AnonymousScratch(
                directory, minimum_free_bytes=admission_free_bytes,
                maximum_file_bytes=maximum_file_bytes,
                maximum_live_handles=maximum_live_handles)
            return scratch, ledger, dict(
                attempts=attempts, waited_seconds=round(clock() - started, 3),
                ledger_slot=ledger.slot)
        except ScratchResourceError as error:
            if ledger is not None:
                ledger.close()
            waited = clock() - started
            if (error.scratch_reason != "scratch_admission" or
                    waited + admission_poll_seconds > admission_wait_seconds):
                error.scratch_details.update(
                    admission_attempts=attempts,
                    admission_waited_seconds=round(waited, 3),
                    admission_wait_seconds=admission_wait_seconds,
                    retryable=error.scratch_reason == "scratch_admission")
                raise
        except BaseException:
            if ledger is not None:
                ledger.close()
            raise
        sleep(admission_poll_seconds)


class ScratchFloorMonitor:
    """Fail-closed guard for a shared node-root ``/tmp``.

    Each poll publishes this owner's accounted bytes and reads ``statvfs``.
    Below ``reserve_bytes`` exactly one participant, the live one with the most
    accounted bytes (ties: highest slot), terminates itself; the others wait
    while any participant is already terminating. Below ``hard_floor_bytes``
    every participant terminates regardless, which also covers unmanaged
    writers and unaccounted solver-internal files. ``on_trigger`` is called at
    most once, from the monitor thread.
    """

    def __init__(self, scratch, ledger, *, directory, reserve_bytes,
                 hard_floor_bytes, poll_seconds, on_trigger, statvfs=None):
        if not 0 < hard_floor_bytes < reserve_bytes:
            raise ValueError("hard floor must be positive and below the reserve")
        self.scratch = scratch
        self.ledger = ledger
        self.directory = str(directory)
        self.reserve_bytes = reserve_bytes
        self.hard_floor_bytes = hard_floor_bytes
        self.poll_seconds = poll_seconds
        self.on_trigger = on_trigger
        self.statvfs = statvfs or os.statvfs
        self.triggered = None
        self.polls = 0
        self.minimum_available_bytes = None
        self.maximum_accounted_bytes = 0
        self.maximum_participants = 0
        self.thread = None
        self.stopping = None
        self.error = None

    def evaluate(self):
        """One poll. Returns the trigger details when this owner must stop."""
        if self.triggered is not None:
            return None
        used = self.scratch.accounted_bytes() or 0
        self.ledger.publish(used)
        status = self.statvfs(self.directory)
        available = status.f_bavail * status.f_frsize
        self.polls += 1
        self.maximum_accounted_bytes = max(self.maximum_accounted_bytes, used)
        if (self.minimum_available_bytes is None or
                available < self.minimum_available_bytes):
            self.minimum_available_bytes = available
        if available >= self.reserve_bytes:
            return None
        participants = self.ledger.participants()
        self.maximum_participants = max(self.maximum_participants, len(participants))
        own = self.ledger.slot
        if available < self.hard_floor_bytes:
            trigger = "hard_floor"
        else:
            others = [p for p in participants if p["slot"] != own]
            if any(p["state"] == "terminating" for p in others):
                return None
            candidates = others + [dict(slot=own, used_bytes=used, state="running")]
            victim = max(candidates, key=lambda p: (p["used_bytes"], p["slot"]))
            if victim["slot"] != own:
                return None
            trigger = "node_floor"
        self.ledger.publish(used, "terminating")
        self.triggered = dict(
            trigger=trigger, available_bytes=available,
            reserve_bytes=self.reserve_bytes,
            hard_floor_bytes=self.hard_floor_bytes, accounted_bytes=used,
            ledger_slot=own, participants=participants, retryable=True)
        return self.triggered

    def _run(self):
        try:
            while not self.stopping.wait(self.poll_seconds):
                details = self.evaluate()
                if details is not None:
                    self.on_trigger(dict(details))
                    return
        except BaseException as error:  # fail closed: a blind guard stops the task
            self.error = repr(error)
            self.triggered = dict(trigger="monitor_failure", error=self.error,
                                  retryable=True)
            self.on_trigger(dict(self.triggered))

    def start(self):
        import threading
        self.stopping = threading.Event()
        self.thread = threading.Thread(target=self._run, name="scratch-floor",
                                       daemon=True)
        self.thread.start()

    def stop(self):
        if self.thread is not None:
            self.stopping.set()
            self.thread.join()
            self.thread = None

    def status(self):
        return dict(polls=self.polls,
                    minimum_available_bytes=self.minimum_available_bytes,
                    maximum_accounted_bytes=self.maximum_accounted_bytes,
                    maximum_participants_below_reserve=self.maximum_participants,
                    triggered=self.triggered)
