#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=sc24-26-quota-governor
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=7-00:00:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-rerun-quota-governor/governor.out
export PYTHONDONTWRITEBYTECODE=1
exec /cm/local/apps/python3/bin/python3 /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-rerun-quota-governor/quota_governor.py --dir /home/hwu/SAT-solving/benchmarks/baselines/sc24-26-rerun-quota-governor --interval 600 --hold-above 750 --release-below 700
