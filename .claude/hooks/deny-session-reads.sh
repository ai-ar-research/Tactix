#!/usr/bin/env bash
# PreToolUse guard: deny any agent tool call that references the user's private
# session-*.txt transcripts (e.g. session-2026-07-17.txt in the repo root).
#
# These files are the USER's own reference notes. They stay on disk but must be
# unreadable to agents (Read / Bash cat|grep|head|tail / Grep / Glob), so the
# next session stays completely unaware of them. See working-style memory.
#
# Reads the PreToolUse hook JSON on stdin; emits a deny decision when any string
# in tool_input matches a session transcript path/command, otherwise stays
# silent (exit 0 = allow).
set -euo pipefail

input="$(cat)"

# Collect every string value inside tool_input (works across Read.file_path,
# Bash.command, Grep.pattern/path/glob, Glob.pattern/path, ...).
refs="$(printf '%s' "$input" | jq -r '[.tool_input | .. | strings] | join(" ")' 2>/dev/null || true)"

if printf '%s' "$refs" | grep -qiE 'session-.*\.txt'; then
  printf '%s' '{"hookSpecificOutput":{"hookEventName":"PreToolUse","permissionDecision":"deny","permissionDecisionReason":"session-*.txt are the user private session transcripts and must not be read by agents. They are intentionally off-limits; do not attempt to read, cat, grep, or glob them. All handoff information lives in PROGRESS.md and commit messages."}}'
fi

exit 0
