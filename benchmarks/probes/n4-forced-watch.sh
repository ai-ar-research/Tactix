#!/usr/bin/env bash
# Completion watcher for the n4-forced array (slurm-jobs skill).
set -u
JID="$1"
while [ "$(squeue -u hwu -h -o '%j' | grep -c n4forced)" != "0" ]; do sleep 45; done
echo "n4forced array $JID has left the queue"
sacct -j "$JID" -o JobID,State,Elapsed -n | head -40
