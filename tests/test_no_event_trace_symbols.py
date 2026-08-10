#!/usr/bin/env python3
"""Production builds must contain no diagnostic event-trace implementation."""
import pathlib
import subprocess
import sys


binary = pathlib.Path(sys.argv[1])
data = binary.read_bytes()
# Method identifiers can remain in DWARF even when every no-op call is inlined
# away.  Check protocol strings here and linked implementation symbols below;
# together those distinguish a real trace implementation from debug metadata.
for marker in (b"TACTIX_EVENT_TRACE", b"tactix-event-trace"):
    assert marker not in data, f"production binary contains {marker!r}"

symbols = subprocess.run(["nm", "-C", str(binary)], text=True,
                         capture_output=True, check=True).stdout
assert "tactix::EventTrace" not in symbols, "production trace symbols linked"
print("test_no_event_trace_symbols OK")
