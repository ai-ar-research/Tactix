#!/usr/bin/env python3
"""Keep deterministic feedback statistics complete; native wall timers stay separate."""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
INVENTORY = ROOT / "src/strategy/feedback_statistics.inc"


def fields():
    kernel = (ROOT / "src/kernel/kernel.hpp").read_text()
    start = kernel.index("struct Stats {")
    end = kernel.index("\n};", start)
    body = re.sub(r"//[^\n]*|/\*.*?\*/", "", kernel[start:end], flags=re.S)
    result = []
    for declaration in re.findall(r"(?<!constexpr )\b(?:uint64_t|uint32_t)\s+([^;]+);", body):
        for part in declaration.split(","):
            match = re.match(r"\s*(\w+)\s*((?:\[[^]]+\])*)\s*=", part)
            if match and not match[1].endswith("_nanos") and not match[1].startswith("MAX_"):
                result.append(("ARRAY" if match[2] else "SCALAR", match[1]))
    return result


def expected():
    return ("// Deterministic native measurements; generated from Stats.\n"
            "// Regenerate with tools/docs/check-feedback-statistics.py --update.\n"
            "// Native wall timers are nondeterministic and remain in the snapshot.\n" +
            "".join(f"TACTIX_FEEDBACK_{kind}({field})\n" for kind, field in fields()))


def main():
    wanted = expected()
    if sys.argv[1:] == ["--update"]:
        INVENTORY.write_text(wanted)
    elif sys.argv[1:]:
        raise SystemExit("usage: check-feedback-statistics.py [--update]")
    actual = INVENTORY.read_text()
    if actual != wanted:
        print("feedback statistics inventory differs from native Stats; regenerate it")
        return 1
    print(f"feedback statistics inventory: {len(fields())} native fields, no wall timers")
    return 0


if __name__ == "__main__":
    sys.exit(main())
