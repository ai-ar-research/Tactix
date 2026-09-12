#!/usr/bin/env python3
"""Production builds must contain no diagnostic trace implementation."""
import pathlib
import re
import subprocess
import sys


binary = pathlib.Path(sys.argv[1])
assert sys.argv[2:] in ([], ["--debug-stubs"])
debug_stubs = bool(sys.argv[2:])
data = binary.read_bytes()
# Method identifiers can remain in DWARF even when every no-op call is inlined
# away.  Check protocol strings here and linked implementation symbols below;
# together those distinguish a real trace implementation from debug metadata.
for marker in (b"TACTIX_EVENT_TRACE", b"tactix-event-trace",
               b"TACTIX_RULE_JOURNAL", b"tactix-rule-journal"):
    assert marker not in data, f"production binary contains {marker!r}"
# ConflictAnalysis's per-conflict journal candidate is deliberately lazy.  An
# exact standalone copy means the production build evaluated the diagnostic
# lambda and retained its dynamic string prefix in the search hot path.
for candidate in (b"conflict=\0", b"victims=\0", b"keep-level=\0"):
    assert candidate not in data, \
        f"production retained hot journal candidate {candidate!r}"

symbols = subprocess.run(["nm", "-C", str(binary)], text=True,
                         capture_output=True, check=True).stdout
# Optimized runtime builds retain the original zero-symbol requirement.
# At -O0 the disabled header's empty inline methods have weak out-of-line
# definitions. Permit only that explicit inventory, never arbitrary weak
# diagnostics or a real implementation. Protocol/candidate strings above
# remain forbidden in both modes.
stub_methods = {
    "checkpoint", "mode_clock", "heap_scores", "reorder_end", "implications",
    "open_from_env", "reorder_begin", "restart_clock", "heap_after_decision",
    "close", "macro", "detail", "learned", "conflict", "reduction",
}
def check_symbols(text, allow_stubs):
    for line in text.splitlines():
        if "tactix::EventTrace" not in line and "rule_journal" not in line:
            continue
        match = re.fullmatch(r"\S+\s+([A-Za-z])\s+(.*)", line.strip())
        assert allow_stubs and match and match[1] == "W", "diagnostic implementation linked"
        symbol = match[2]
        method = re.match(r"tactix::EventTrace::([a-z_]+)\(", symbol)
        assert (method and method[1] in stub_methods) or symbol == "tactix::rule_journal_close()", \
            "unrecognized diagnostic symbol"

for allowed in (False, True):
    check_symbols("0000 T main", allowed)
    for rejected in ("0000 T tactix::EventTrace::macro(tactix::Kernel const&)",
                     "0000 W tactix::EventTrace::emit_record()",
                     "0000 W tactix::rule_journal_emit()"):
        try:
            check_symbols(rejected, allowed)
        except AssertionError:
            pass
        else:
            raise AssertionError("negative symbol control accepted")
check_symbols("0000 W tactix::EventTrace::close()", True)
try:
    check_symbols("0000 W tactix::EventTrace::close()", False)
except AssertionError:
    pass
else:
    raise AssertionError("optimized zero-symbol gate weakened")
check_symbols(symbols, debug_stubs)
print("test_no_event_trace_symbols OK")
