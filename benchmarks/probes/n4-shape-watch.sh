#!/usr/bin/env bash
# Completion watcher for the n4-shape array (slurm-jobs skill: arm a watcher for
# every submitted job). Fires once the job name leaves the queue, whatever the
# outcome, and prints the per-task states so a failure is visible too.
set -u
JID="$1"
while [ "$(squeue -u hwu -h -o '%j' | grep -c n4shape)" != "0" ]; do sleep 45; done
echo "n4shape array $JID has left the queue"
sacct -j "$JID" -o JobID,State,Elapsed -n | head -80
