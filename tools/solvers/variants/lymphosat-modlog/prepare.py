#!/usr/bin/env python3
"""Materialize the LymphoSAT attribution-logging variant from the frozen pin.

Copies the frozen SC24-26 baseline LymphoSAT pin, overlays the patched
dispatcher (`overlay/main.cpp`, which only adds `c composite: solved by ...`
and `c composite: dispatching fallback ...` stderr lines), and rebuilds the
composite dispatcher with the same portable-core contract used to build the
pin (g++ 13.1.0 through the `lymphosat-generic-cxx` flag filter with
`-march=x86-64-v2 -mtune=generic`; see tools/bench/build_lymphosat_portable.py).
All 127 nested solver binaries are reused byte-identical from the pin, which
the script attests before finishing.
"""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASE_PIN = ROOT / "benchmarks/baselines/sc24-26-32g-5000s/pins/lymphosat"
ADAPTER = ROOT / "tools/bench/lymphosat-generic-cxx"
# solver/core/main.cpp of the frozen pin; the overlay was derived from exactly
# this file, so refuse a base that drifted.
PRISTINE_MAIN_SHA = "09630b9ae6ad32c877a16ca3ce9e05a8d4af8cd344516955d10eb6897892d368"


def sha256(path):
    import hashlib
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    dest = HERE / "tree"
    if dest.exists():
        raise SystemExit(f"refusing to overwrite existing generated tree: {dest}")
    if sha256(BASE_PIN / "solver/core/main.cpp") != PRISTINE_MAIN_SHA:
        raise SystemExit("frozen pin main.cpp does not match the overlay base")
    shutil.copytree(BASE_PIN, dest)
    subprocess.run(["chmod", "-R", "u+w", dest], check=True)
    shutil.copy2(HERE / "overlay/main.cpp", dest / "solver/core/main.cpp")

    wrapper = pathlib.Path(tempfile.mkdtemp(prefix="lymphosat-modlog-cxx-"))
    build = pathlib.Path(tempfile.mkdtemp(prefix="lymphosat-modlog-build-"))
    try:
        (wrapper / "g++").symlink_to(ADAPTER)
        (wrapper / "clang++").symlink_to(ADAPTER)
        env = os.environ.copy()
        env.update({"PATH": f"{wrapper}:{env['PATH']}", "CC": "gcc",
                    "CXX": "g++",
                    "CXXFLAGS": "-march=x86-64-v2 -mtune=generic"})
        subprocess.run(["cmake", "-S", dest / "solver", "-B", build,
                        "-DCMAKE_BUILD_TYPE=Release"], env=env, check=True)
        subprocess.run(["cmake", "--build", build, "--target",
                        "lymphosat_composite", "--parallel", "4"],
                       env=env, check=True)
    finally:
        shutil.rmtree(wrapper, ignore_errors=True)
        shutil.rmtree(build, ignore_errors=True)

    diff = subprocess.run(["diff", "-r", "--brief", BASE_PIN, dest],
                          capture_output=True, text=True)
    changed = sorted(line.split()[3] for line in diff.stdout.splitlines())
    expected = sorted([str(dest / "solver/core/main.cpp"),
                       str(dest / "solver/solver")])
    if changed != expected:
        raise SystemExit(f"unexpected divergence from frozen pin: {changed}")
    print("composite sha256:", sha256(dest / "solver/solver"))
    print("frozen pin composite sha256:", sha256(BASE_PIN / "solver/solver"))
    print("only dispatcher source and binary differ from the frozen pin")


if __name__ == "__main__":
    sys.exit(main() or 0)
