#!/usr/bin/env bash
# Watcher: block until no job named $1 is in the queue, then report.
set -u
name="$1"
until [ "$(squeue -u hwu -h -o '%j' | grep -c "$name")" = "0" ]; do sleep 20; done
echo "NO JOB NAMED $name IN QUEUE"
date -Is
