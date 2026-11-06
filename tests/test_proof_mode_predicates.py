#!/usr/bin/env python3
"""The proof mode stays out of the search (N2).

A run with no proof is a Proof without a writer, not a mode the search can ask
about. Only tactix_proof_route reads `Proof::writes()`, and only
finish_terminal_certificate reads `Proof::terminal_certificate()`; the ledger
and its writer define them. No identifier names the retired `nowhere` mode
except `open_nowhere`, which opens a proof without a writer.
"""

from pathlib import Path
import re
import sys


root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parents[1]
src = root / "src"

owners = {
    Path("src/proof/proof.hpp"),
    Path("src/proof/proof.cpp"),
    Path("src/proof/proof_writer.hpp"),
    Path("src/proof/proof_writer.cpp"),
}
# Each predicate, and the one function outside its owners that may read it.
readers = {
    "writes()": (
        re.compile(r"\bwrites\s*\(\s*\)"),
        (Path("src/api/api.cpp"), "tactix_proof_route"),
    ),
    "terminal_certificate()": (
        re.compile(r"(?<!\w)terminal_certificate\s*\("),
        (Path("src/rules/lucky/refutation_proof_sink.hpp"),
         "finish_terminal_certificate"),
    ),
}
nowhere = re.compile(r"\w*nowhere\w*")
# A namespace-scope line that opens a definition: the name before its `(`.
definition = re.compile(r"^(?![\s#/}])[^;]*?\b([A-Za-z_]\w*)\s*\(")


def without_comments(text: str) -> str:
    """`text` with comments blanked and line breaks kept."""
    out = []
    at, n = 0, len(text)
    while at < n:
        if text.startswith("//", at):
            end = text.find("\n", at)
            end = n if end < 0 else end
            out.append(" " * (end - at))
            at = end
        elif text.startswith("/*", at):
            end = text.find("*/", at + 2)
            end = n if end < 0 else end + 2
            out.append(re.sub(r"[^\n]", " ", text[at:end]))
            at = end
        elif text[at] in "\"'":
            quote, start = text[at], at
            at += 1
            while at < n and text[at] != quote and text[at] != "\n":
                at += 2 if text[at] == "\\" else 1
            at = min(at + 1, n)
            out.append(text[start:at])
        else:
            out.append(text[at])
            at += 1
    return "".join(out)


def enclosing(lines: list[str], index: int) -> str:
    for line in reversed(lines[: index + 1]):
        found = definition.match(line)
        if found:
            return found.group(1)
    return ""


violations = []
for path in sorted(src.rglob("*")):
    if path.suffix not in {".cpp", ".hpp", ".h", ".c", ".inc"}:
        continue
    relative = path.relative_to(root)
    text = path.read_text(errors="replace")
    for number, line in enumerate(text.splitlines(), 1):
        for word in nowhere.findall(line):
            if word != "open_nowhere":
                violations.append(f"{relative}:{number}: `{word}` names the retired mode")
    if relative in owners:
        continue
    code = without_comments(text).splitlines()
    for name, (pattern, (reader, function)) in readers.items():
        for index, line in enumerate(code):
            if not pattern.search(line):
                continue
            if relative == reader and enclosing(code, index) == function:
                continue
            violations.append(
                f"{relative}:{index + 1}: reads {name}; only {function} "
                f"({reader}) may")

if violations:
    print("\n".join(violations))
    raise SystemExit("proof mode predicates: FAIL")
print("proof mode predicates: ok")
