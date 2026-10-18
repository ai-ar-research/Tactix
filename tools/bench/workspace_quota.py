"""Stop a workspace-backed proof writer at the agreed home quota threshold.

This is a hard usage limit, not a reserved amount of free scratch. The Slurm
governor separately holds new array tasks while usage is above the threshold.
"""
import getpass
from pathlib import Path
import re
import threading


def used_gib(table, user):
    for line in table.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0] == user:
            match = re.fullmatch(r"([0-9]+(?:\.[0-9]+)?)([KMGTP]?)", fields[1])
            if not match:
                raise ValueError("unrecognized home quota usage")
            value, unit = match.groups()
            return float(value) * {"": 1 / 2**30, "K": 1 / 2**20,
                                   "M": 1 / 2**10, "G": 1, "T": 1024,
                                   "P": 1024**2}[unit]
    raise ValueError("user missing from home quota table")


class WorkspaceQuotaMonitor:
    def __init__(self, on_limit, *, threshold_gib=750, poll_seconds=2,
                 quota_path=Path("/home/.zfs-quota"), user=None):
        self.on_limit = on_limit
        self.threshold_gib = threshold_gib
        self.poll_seconds = poll_seconds
        self.quota_path = Path(quota_path)
        self.user = user or getpass.getuser()
        self.stop_event = threading.Event()
        self.thread = None
        self.triggered = None
        self.minimum_headroom_gib = None

    def poll(self):
        try:
            usage = used_gib(self.quota_path.read_text(), self.user)
        except (OSError, ValueError) as error:
            self.triggered = {"reason": "quota_unavailable", "detail": str(error)[:120]}
        else:
            headroom = self.threshold_gib - usage
            if self.minimum_headroom_gib is None or headroom < self.minimum_headroom_gib:
                self.minimum_headroom_gib = headroom
            if usage > self.threshold_gib:
                self.triggered = {"reason": "quota_limit", "usage_gib": usage,
                                  "threshold_gib": self.threshold_gib}
        if self.triggered is not None:
            self.on_limit(self.triggered)

    def start(self):
        self.poll()
        if self.triggered is not None:
            return
        def run():
            while not self.stop_event.wait(self.poll_seconds):
                self.poll()
                if self.triggered is not None:
                    return
        self.thread = threading.Thread(target=run, name="workspace-quota", daemon=True)
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        if self.thread is not None:
            self.thread.join(timeout=self.poll_seconds + 5)
