#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=4
#SBATCH --mem=32768M
#SBATCH --no-requeue
#SBATCH --time=00:35:00
#SBATCH --job-name=r541-binary-lsr
#SBATCH --output=/home/hwu/SAT-solving/build/pin-r541-binary-lsr-probe-ee601c089/slurm-%j.log
set -euo pipefail
ulimit -c 0
exec python3 /home/hwu/SAT-solving/build/pin-r541-binary-lsr-probe-ee601c089/probe.py
