#!/usr/bin/env python3
"""Build the standalone public-API EEVBNN policy without changing CMake files."""

import argparse
import os
import shlex
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SOURCE = os.path.join(ROOT, "examples", "eevbnn_policy.c")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", default=os.path.join(ROOT, "build", "release"))
    parser.add_argument("--output", default=os.path.join(
        ROOT, "corpus", "eevbnn", "bin", "tactix-eevbnn-policy"))
    args = parser.parse_args()
    build = os.path.abspath(args.build)
    link_file = os.path.join(build, "CMakeFiles", "tactix.dir", "link.txt")
    if not os.path.isfile(link_file):
        raise SystemExit(f"missing configured Tactix build: {link_file}")
    with open(link_file, encoding="utf-8") as source:
        link = shlex.split(source.read())
    main_object = "CMakeFiles/tactix.dir/src/main.cpp.o"
    try:
        index = link.index(main_object)
    except ValueError as error:
        raise SystemExit(f"unexpected link command (no {main_object})") from error
    output = os.path.abspath(args.output)
    os.makedirs(os.path.dirname(output), exist_ok=True)
    object_file = output + ".o"
    subprocess.run([
        "gcc", "-std=c11", "-O3", "-DNDEBUG", "-I", os.path.join(ROOT, "include"),
        "-c", SOURCE, "-o", object_file,
    ], check=True)
    link[index] = object_file
    out_index = link.index("-o") + 1
    link[out_index] = output
    subprocess.run(link, cwd=build, check=True)
    print(output)


if __name__ == "__main__":
    main()
