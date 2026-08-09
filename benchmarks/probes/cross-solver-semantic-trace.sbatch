#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=cross-trace
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:05:00
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/cross-trace-%j.out
exec /home/hwu/SAT-solving/benchmarks/probes/cross-solver-semantic-trace.sh
