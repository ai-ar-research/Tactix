#!/usr/bin/env bash
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --job-name=tierd-storage-audit
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=1G
#SBATCH --time=00:05:00
#SBATCH --no-requeue
#SBATCH --output=/home/hwu/SAT-solving/benchmarks/baselines/sc24-26-32g-5000s/logs/storage-audit.log
set -eu
hostname
df -hT
findmnt -rn -o TARGET,FSTYPE,SOURCE | head -100
lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINTS
