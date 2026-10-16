#!/usr/bin/env bash
# System packages for building and running the artifact on Ubuntu 24.04
# (the Docker image and Apptainer recipe call this; on a VM, run it with sudo).
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
# Run apt as root throughout: rootless builds (Apptainer --fakeroot) cannot
# switch to apt's sandbox user.
APT="apt-get -o APT::Sandbox::User=root"
$APT update
$APT install -y --no-install-recommends \
    build-essential cmake git curl ca-certificates xz-utils bzip2 unzip \
    zlib1g-dev libboost-all-dev python3 python3-matplotlib make time bc
rm -rf /var/lib/apt/lists/*
# Lean's toolchain manager, for building the Trestle proof checker.
if ! command -v elan >/dev/null 2>&1; then
    curl -sSfL https://raw.githubusercontent.com/leanprover/elan/master/elan-init.sh \
        | sh -s -- -y --default-toolchain none --no-modify-path
fi
