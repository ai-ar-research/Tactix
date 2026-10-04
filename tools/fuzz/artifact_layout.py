#!/usr/bin/env python3
"""Choose a collision-free output directory for one fuzz job and leg."""

import argparse
import os
import re


_COMPONENT = re.compile(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}\Z")


def validate_component(value, label):
    if not _COMPONENT.fullmatch(value):
        raise ValueError(
            f"{label} must match [A-Za-z0-9][A-Za-z0-9._-]{{0,127}}: "
            f"{value!r}")
    return value


def output_dir(base, run_id, leg_id):
    """Return the deterministic private artifact directory for a run leg."""
    run = validate_component(run_id, "run id")
    leg = validate_component(leg_id, "leg id")
    return os.path.join(os.path.abspath(base), "runs", run, leg)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("base")
    parser.add_argument("run_id")
    parser.add_argument("leg_id")
    args = parser.parse_args()
    try:
        print(output_dir(args.base, args.run_id, args.leg_id))
    except ValueError as exc:
        parser.error(str(exc))


if __name__ == "__main__":
    main()
