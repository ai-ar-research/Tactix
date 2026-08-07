#!/usr/bin/env python3
"""Build-path ownership, enforced by the filesystem instead of by memory.

The rule (CLAUDE.md): a submitted SLURM array owns its binary path until it
finishes. Rebuilding into that path swaps the binary out from under running
tasks, and the arm measured is then not the arm it is taken to be. The rule was
broken repeatedly by people who knew it, so the wrong action must fail rather
than warn.

Acquiring a lock writes a human-readable `BUILD-DIR-OWNED` marker naming the
owning job, then strips write permission from the whole build tree
(`chmod -R a-w`). Any subsequent write fails — `cmake --build`, a bare `make`,
`ninja`, `cp`, `install`, anything. A guard that fires only inside a wrapper
script is bypassed by the most natural command, which is the command that caused
every violation. Read and execute are untouched, so a running array keeps using
the binary normally.

The lock is validated, never trusted, so it cannot wedge a directory. Every
operation re-asks SLURM whether the owning job is still live:
  * job still in the queue      -> release refuses (the case worth protecting)
  * job gone (finished/killed/  -> the lock is stale and releases freely, and
    cancelled/never submitted)     `--gc` clears it without being asked
  * squeue unavailable or the   -> release is allowed with a warning: a stale
    marker unreadable/malformed    lock that blocks all future work is the worse
                                   failure, so every ambiguous case resolves
                                   toward release.
Permissions are the owner's to change in any case (`chmod` always works), so no
state this creates is unrecoverable by hand either.

Normal use is automatic: `run.py --slurm` calls `--gc` before submitting and
`--acquire` right after, and the dependent aggregate job calls `--release` when
the array finishes. Hand-rolled `sbatch` probes should call `--acquire`
themselves; see the slurm-jobs skill.

Reclaiming a directory by hand:
    python3 tools/bench/buildlock.py --check   build/pin-foo   # why is it locked?
    python3 tools/bench/buildlock.py --release build/pin-foo   # give it back
    python3 tools/bench/buildlock.py --gc                      # clear all stale
    python3 tools/bench/buildlock.py --list                    # show every lock
`--release` on a directory whose job is still running refuses and names the job
id; `--force` overrides it, and is right only once the array's results are
being discarded.

Sweeping stale `build/pin-*` dirs: a locked tree is read-only, so `rm -rf`
fails on it. Run `--gc` (or `--release`) first, then sweep.
"""
import argparse
import getpass
import json
import os
import stat
import subprocess
import sys
import time

MARKER = "BUILD-DIR-OWNED"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(__file__))))
ROOT = subprocess.run(["git", "rev-parse", "--show-toplevel"],
                      capture_output=True, text=True,
                      cwd=os.path.dirname(os.path.abspath(__file__))
                      ).stdout.strip() or os.getcwd()

LIVE, DEAD, UNKNOWN = "live", "dead", "unknown"


def job_state(jobid):
    """Is `jobid` still in the queue? Returns LIVE / DEAD / UNKNOWN.

    UNKNOWN means the question could not be asked (no squeue, or it errored in
    a way that is not "no such job"). Callers must treat UNKNOWN as releasable
    — see the module docstring: never wedge."""
    if not jobid:
        return DEAD                      # no owner recorded: nothing to protect
    try:
        r = subprocess.run(["squeue", "-h", "-j", str(jobid), "-o", "%i"],
                           capture_output=True, text=True, timeout=30)
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError):
        return UNKNOWN
    if r.stdout.strip():
        return LIVE
    # squeue exits non-zero with "Invalid job id specified" once a job has aged
    # out of the queue. That is the ordinary finished case, not an error.
    if r.returncode != 0 and "invalid job id" not in r.stderr.lower():
        return UNKNOWN
    return DEAD


def read_marker(d):
    p = os.path.join(d, MARKER)
    if not os.path.exists(p):
        return None
    try:
        with open(p) as f:
            txt = f.read()
        return json.loads(txt[txt.index("{"):])
    except Exception:
        # An unparsable marker still means "someone locked this", but with no
        # verifiable owner it must stay releasable. Report no job id.
        return {"job": None, "unparsable": True}


def chmod_tree(d, writable):
    """Strip or restore owner write across the whole tree.

    Bottom-up on acquire so a directory is not made read-only before its
    contents are processed; top-down on release so a directory is writable
    before its contents are touched."""
    entries = []
    for dirpath, dirnames, filenames in os.walk(d):
        entries.append(dirpath)
        entries.extend(os.path.join(dirpath, f) for f in filenames)
    if not writable:
        entries.reverse()
    for p in entries:
        try:
            mode = os.lstat(p).st_mode
            if stat.S_ISLNK(mode):
                continue
            if writable:
                os.chmod(p, stat.S_IMODE(mode) | stat.S_IWUSR)
            else:
                os.chmod(p, stat.S_IMODE(mode) & ~(stat.S_IWUSR | stat.S_IWGRP
                                                   | stat.S_IWOTH))
        except OSError as e:
            print(f"buildlock: warning: chmod {p}: {e}", file=sys.stderr)


# The permission strip is the guarantee: it stops every tool, including ones
# nobody thought of. A bare "Permission denied" does not say why, though, so the
# generator's entry point is swapped for a stub that prints the marker and
# fails. The stub only carries the message — an unrecognised generator or a
# failed swap still leaves permissions blocking the build.
GEN_ENTRY = ("Makefile", "build.ninja")
SAVED = ".buildlock-orig"

# Every target must depend on FORCE. Without it `make tactix` matches the `%:`
# pattern, sees the existing binary, prints "'tactix' is up to date" and exits
# 0 — a locked directory reporting a successful build.
_MAKE_STUB = """# Installed by tools/bench/buildlock.py — see BUILD-DIR-OWNED.
# The real Makefile is saved next to this one as Makefile{saved}.
.DEFAULT_GOAL := __buildlock__
.PHONY: __buildlock__ FORCE
__buildlock__: FORCE
\t@cat {marker} >&2; exit 1
%: FORCE
\t@cat {marker} >&2; exit 1
FORCE:
"""

_NINJA_STUB = """# Installed by tools/bench/buildlock.py — see BUILD-DIR-OWNED.
rule buildlock
  command = cat {marker} >&2; exit 1
  description = BUILD DIRECTORY LOCKED BY A RUNNING SLURM JOB
build __buildlock__: buildlock
default __buildlock__
"""


def swap_entry(d, lock):
    """Install (lock=True) or restore (lock=False) the generator stub."""
    for name in GEN_ENTRY:
        live, saved = os.path.join(d, name), os.path.join(d, name + SAVED)
        try:
            if lock:
                if not os.path.exists(live) or os.path.exists(saved):
                    continue
                os.rename(live, saved)
                stub = _MAKE_STUB if name == "Makefile" else _NINJA_STUB
                with open(live, "w") as f:
                    f.write(stub.format(marker=MARKER, saved=SAVED))
            else:
                if not os.path.exists(saved):
                    continue
                if os.path.exists(live):
                    os.unlink(live)
                os.rename(saved, live)
        except OSError as e:
            print(f"buildlock: warning: {'installing' if lock else 'restoring'}"
                  f" {name}: {e} (permissions still guard the tree)",
                  file=sys.stderr)


def status(d):
    """(state, marker) for one directory."""
    m = read_marker(d)
    if m is None:
        return "free", None
    return job_state(m.get("job")), m


def acquire(d, jobid, note):
    d = os.path.abspath(d)
    if not os.path.isdir(d):
        print(f"buildlock: {d} is not a directory — nothing to lock")
        return 1
    st, m = status(d)
    if st == LIVE and str(m.get("job")) != str(jobid):
        print(f"buildlock: REFUSING — {d} is already owned by LIVE job "
              f"{m.get('job')}. Build somewhere else (that is the rule), or "
              f"release it deliberately if that job's results are being "
              f"discarded.", file=sys.stderr)
        return 2
    if st in (DEAD, UNKNOWN) and m is not None:
        chmod_tree(d, writable=True)     # reclaim a stale lock silently
        swap_entry(d, lock=False)
    payload = {"job": str(jobid), "user": getpass.getuser(),
               "host": os.uname().nodename, "acquired": time.strftime("%FT%T%z"),
               "note": note or "", "dir": d}
    with open(os.path.join(d, MARKER), "w") as f:
        f.write(
            f"THIS BUILD DIRECTORY IS OWNED BY SLURM JOB {jobid} AND IS "
            f"READ-ONLY.\n\n"
            f"Rebuilding here would swap the binary out from under the running\n"
            f"tasks, so every write into this tree is blocked by permissions.\n"
            f"Build your change into a FRESH build/pin-<experiment>/ instead.\n\n"
            f"To reclaim it once job {jobid} has finished:\n"
            f"    python3 tools/bench/buildlock.py --release {d}\n"
            f"(it re-checks SLURM and refuses only while the job is still live)\n"
            f"    python3 tools/bench/buildlock.py --gc     # clears ALL stale locks\n\n"
            + json.dumps(payload, indent=2) + "\n")
    swap_entry(d, lock=True)             # the messenger
    chmod_tree(d, writable=False)        # the guarantee
    print(f"buildlock: LOCKED {d} for job {jobid}")
    return 0


def release(d, force):
    d = os.path.abspath(d)
    st, m = status(d)
    if m is None:
        print(f"buildlock: {d} is not locked")
        return 0
    if st == LIVE and not force:
        print(f"buildlock: REFUSING to release {d} — job {m.get('job')} is "
              f"still RUNNING and is using this binary. Re-run with --force "
              f"only if you are discarding that array's results.",
              file=sys.stderr)
        return 2
    if st == UNKNOWN:
        print(f"buildlock: warning: could not verify job {m.get('job')} with "
              f"squeue; releasing anyway (a wedged directory is worse).",
              file=sys.stderr)
    chmod_tree(d, writable=True)
    swap_entry(d, lock=False)
    try:
        os.unlink(os.path.join(d, MARKER))
    except OSError:
        pass
    print(f"buildlock: released {d}" + (" (FORCED)" if force and st == LIVE else ""))
    return 0


def locked_dirs():
    base = os.path.join(ROOT, "build")
    if not os.path.isdir(base):
        return []
    out = []
    for n in sorted(os.listdir(base)):
        d = os.path.join(base, n)
        if os.path.isdir(d) and os.path.exists(os.path.join(d, MARKER)):
            out.append(d)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--acquire", nargs="+", metavar="DIR")
    ap.add_argument("--release", nargs="+", metavar="DIR")
    ap.add_argument("--check", nargs="+", metavar="DIR")
    ap.add_argument("--gc", action="store_true",
                    help="release every lock whose owning job is gone")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--job", help="owning SLURM job id (with --acquire)")
    ap.add_argument("--note", default="", help="free text stored in the marker")
    ap.add_argument("--force", action="store_true",
                    help="with --release: release even while the job is live")
    a = ap.parse_args()

    rc = 0
    if a.acquire:
        if not a.job:
            ap.error("--acquire needs --job <slurm job id>")
        for d in a.acquire:
            rc |= acquire(d, a.job, a.note)
    if a.release:
        for d in a.release:
            rc |= release(d, a.force)
    if a.check:
        for d in a.check:
            st, m = status(os.path.abspath(d))
            if m is None:
                print(f"{d}: FREE")
            else:
                print(f"{d}: {'OWNED' if st == LIVE else 'STALE'} "
                      f"(job {m.get('job')}, {st})"
                      + ("" if st == LIVE else " -> --release will free it"))
    if a.gc:
        n = 0
        for d in locked_dirs():
            st, m = status(d)
            if st != LIVE:
                release(d, force=False)
                n += 1
        print(f"buildlock: gc released {n} stale lock(s)")
    if a.list:
        found = locked_dirs()
        if not found:
            print("buildlock: no locked build directories")
        for d in found:
            st, m = status(d)
            print(f"  {os.path.relpath(d, ROOT):28} job {str(m.get('job')):>10}  {st}")
    if not any([a.acquire, a.release, a.check, a.gc, a.list]):
        ap.print_help()
    return rc


if __name__ == "__main__":
    sys.exit(main())
