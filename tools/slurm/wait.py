#!/usr/bin/env python3
"""Wait for one explicit SLURM job and summarize its terminal state."""

import argparse
import datetime
import re
import subprocess
import time
from pathlib import Path


def output(*argv):
    return subprocess.run(argv, text=True, capture_output=True, check=True).stdout.strip()


def accounting_state(text, job, since=None):
    """Do not mix incarnations of a recycled numeric ID or inspect only task0."""
    rows = []
    for line in text.splitlines():
        if not line.strip():
            continue
        fields = line.split('|')
        if len(fields) < 4 or not re.fullmatch(re.escape(job) + r'(?:_\d+)?', fields[0].strip()):
            raise ValueError('unexpected accounting row: ' + line)
        identity, state, exit_code, submitted = (f.strip() for f in fields[:4])
        stamp = datetime.datetime.strptime(submitted, '%Y-%m-%dT%H:%M:%S')
        if stamp.isoformat(timespec='seconds') != submitted or not state:
            raise ValueError('invalid submission time or empty state')
        if since and submitted < since:
            continue
        rows.append((identity, state.split()[0], exit_code, submitted))
    if not rows:
        return None
    if len({row[3] for row in rows}) != 1 or len({row[0] for row in rows}) != len(rows):
        raise ValueError('ambiguous/recycled job ID; use --since with the launch submission time')
    states = {row[1] for row in rows}
    if states & {'PENDING', 'RUNNING', 'CONFIGURING', 'COMPLETING', 'SUSPENDED', 'REQUEUED', 'RESIZING'}:
        return None
    terminals = {'COMPLETED', 'FAILED', 'CANCELLED', 'TIMEOUT', 'OUT_OF_MEMORY',
                 'NODE_FAIL', 'BOOT_FAIL', 'DEADLINE', 'PREEMPTED', 'REVOKED', 'SPECIAL_EXIT'}
    if not states <= terminals:
        raise ValueError('unrecognized accounting states: ' + ','.join(sorted(states - terminals)))
    if states == {'COMPLETED'}:
        return 'COMPLETED' if all(row[2] == '0:0' for row in rows) else 'NONZERO_EXIT'
    return ','.join(sorted(states - {'COMPLETED'}))


def wait_for_job(job, poll, since=None):
    while True:
        try:
            try:
                queued = output('squeue', '-h', '-j', job, '-o', '%i')
            except subprocess.CalledProcessError as error:
                # Purged jobs can make squeue fail instead of returning empty.
                # Only this exact response permits falling through to sacct;
                # disappearance alone is never evidence of solver success.
                if (error.returncode != 1 or (error.stdout or '').strip() or
                        (error.stderr or '').strip() !=
                        'slurm_load_jobs error: Invalid job id specified'):
                    raise
                queued = ''
            if queued:
                time.sleep(max(1, poll))
                continue
            command = ['sacct', '-n', '-X', '--array', '-j', job,
                       '-o', 'JobID,State%40,ExitCode,Submit', '-P']
            if since:
                command += ['-S', since]
            final = accounting_state(output(*command), job, since)
            if final:
                return final
        except subprocess.CalledProcessError as error:
            # A transport/accounting failure is not a terminal solver state.
            print(f'SLURM job {job}: observation failed ({error.returncode}); retrying same job', flush=True)
        time.sleep(max(1, poll))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--job", required=True)
    ap.add_argument("--log")
    ap.add_argument("--poll", type=int, default=30)
    ap.add_argument("--since", help="submission lower bound, YYYY-MM-DDTHH:MM:SS; disambiguates recycled IDs")
    args = ap.parse_args()
    if args.since:
        try:
            parsed = datetime.datetime.strptime(args.since, '%Y-%m-%dT%H:%M:%S')
            if parsed.isoformat(timespec='seconds') != args.since:
                raise ValueError('noncanonical timestamp')
        except ValueError:
            ap.error('--since must be YYYY-MM-DDTHH:MM:SS')
    try:
        final = wait_for_job(args.job, args.poll, args.since)
    except ValueError as error:
        print(f'SLURM job {args.job}: UNVERIFIED ({error})')
        return 2
    print(f"SLURM job {args.job}: {final}")
    if args.log:
        path = Path(args.log)
        if path.exists():
            lines = path.read_text(errors="replace").splitlines()
            print("\n".join(lines[-40:]))
        else:
            print(f"log not found: {path}")
    return 0 if final.startswith("COMPLETED") else 1


if __name__ == "__main__":
    raise SystemExit(main())
