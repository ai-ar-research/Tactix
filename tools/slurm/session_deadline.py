#!/usr/bin/env python3
"""Session-local deadline reminder; never submits, cancels or kills work."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import time


def phase(now, deadline, wind_down_minutes=15):
    if now.tzinfo is None or deadline.tzinfo is None:
        raise ValueError('timezone-aware timestamps required')
    if wind_down_minutes < 0:
        raise ValueError('negative wind-down interval')
    if now >= deadline:
        return 'cutoff'
    if now >= deadline - dt.timedelta(minutes=wind_down_minutes):
        return 'wind_down'
    return 'working'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--deadline', required=True)
    parser.add_argument('--wind-down-minutes', type=int, default=15)
    parser.add_argument('--watch-dir', type=Path)
    args = parser.parse_args()
    deadline = dt.datetime.fromisoformat(args.deadline)
    now = dt.datetime.now(dt.timezone.utc)
    phase(now, deadline, args.wind_down_minutes)  # validate before creating state
    state_dir = args.watch_dir
    if state_dir:
        state_dir.mkdir(parents=True, exist_ok=False)
        config = dict(deadline=deadline.isoformat(), wind_down_minutes=args.wind_down_minutes,
                      pid=os.getpid(), started_at=now.isoformat(),
                      action='Wind down; at cutoff start no new tasks. Finish safe handoff only. No automatic cancellation.')
        with (state_dir / 'deadline.json').open('x') as out:
            json.dump(config, out, indent=2)
            out.write('\n')
    previous = None
    while True:
        now = dt.datetime.now(dt.timezone.utc)
        current = phase(now, deadline, args.wind_down_minutes)
        record = dict(phase=current, checked_at=now.isoformat(), deadline=deadline.isoformat(), pid=os.getpid())
        if state_dir:
            # Only this monitor owns its mutable heartbeat; the config is immutable.
            temporary = state_dir / 'status.next.json'
            temporary.write_text(json.dumps(record, indent=2) + '\n')
            temporary.replace(state_dir / 'status.json')
        if current != previous:
            print(json.dumps(record), flush=True)
            previous = current
        if not state_dir or current == 'cutoff':
            return 0 if current == 'working' else 2
        next_boundary = deadline if current == 'wind_down' else deadline - dt.timedelta(minutes=args.wind_down_minutes)
        time.sleep(min(30, max(0.01, (next_boundary-now).total_seconds())))


if __name__ == '__main__':
    raise SystemExit(main())
