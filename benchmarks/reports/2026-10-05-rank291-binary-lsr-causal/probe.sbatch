#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --time=00:35:00
#SBATCH --job-name=r291-bin-lsr
#SBATCH --output=/home/hwu/SAT-solving/build/pin-r291-binary-lsr-probe-0dfc50a29/logs/%x-%j.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/build/pin-r291-binary-lsr-probe-0dfc50a29/probe.py
