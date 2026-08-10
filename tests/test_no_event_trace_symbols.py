#!/usr/bin/env python3
"""Production builds must contain no diagnostic event-trace implementation."""
import pathlib
import subprocess
import sys


binary = pathlib.Path(sys.argv[1])
data = binary.read_bytes()
for marker in (b"TACTIX_EVENT_TRACE", b"tactix-event-trace",
               b"reorder_begin", b"analysis_end"):
    assert marker not in data, f"production binary contains {marker!r}"

symbols = subprocess.run(["nm", "-C", str(binary)], text=True,
                         capture_output=True, check=True).stdout
assert "tactix::EventTrace" not in symbols, "production trace symbols linked"
print("test_no_event_trace_symbols OK")
