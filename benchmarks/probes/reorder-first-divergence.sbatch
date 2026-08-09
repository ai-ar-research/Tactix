#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=reorder-trace
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem-per-cpu=8G
#SBATCH --time=00:20:00
#SBATCH --array=0-9
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/probes/reorder-trace-%A-%a.out
exec /home/hwu/SAT-solving/benchmarks/probes/reorder-first-divergence.sh
