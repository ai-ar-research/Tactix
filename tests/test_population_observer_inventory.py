#!/usr/bin/env python3
"""Static guard for the authoritative population-observer hook inventory."""

from pathlib import Path
import re
import sys


root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parents[1]
src = root / "src"


def fail(message: str) -> None:
    raise AssertionError(message)


def function_body(path: Path, signature: str) -> str:
    text = path.read_text()
    start = text.find(signature)
    if start < 0:
        fail(f"missing audited endpoint {signature!r} in {path.relative_to(root)}")
    brace = text.find("{", start)
    if brace < 0:
        fail(f"missing body for {signature!r}")
    depth = 0
    for at in range(brace, len(text)):
        if text[at] == "{":
            depth += 1
        elif text[at] == "}":
            depth -= 1
            if depth == 0:
                return text[brace : at + 1]
    fail(f"unterminated body for {signature!r}")
    return ""


def require(path: Path, signature: str, *tokens: str) -> None:
    body = function_body(path, signature)
    for token in tokens:
        if token not in body:
            fail(
                f"{path.relative_to(root)}:{signature} no longer contains "
                f"required population hook {token!r}"
            )


kernel_cpp = src / "kernel" / "kernel.cpp"
kernel_hpp = src / "kernel" / "kernel.hpp"
proof_cpp = src / "proof" / "proof.cpp"
parse_cpp = src / "frontend" / "parse.cpp"

# Clause semantic storage can only be created/replaced/retired at the audited
# kernel primitives. LRAT input replay may only rebind the proof identity.
mutation_patterns = {
    re.compile(r"->flags\s*\|=\s*Clause::GARBAGE"): {kernel_hpp},
    re.compile(r"->size\s*=(?!=)"): {kernel_cpp},
    re.compile(r"->set_id\s*\("): {kernel_cpp, parse_cpp},
}
for pattern, allowed in mutation_patterns.items():
    hits = set()
    for path in src.rglob("*"):
        if path.suffix not in {".cpp", ".hpp"}:
            continue
        if pattern.search(path.read_text(errors="ignore")):
            hits.add(path)
    if hits != allowed:
        fail(
            f"semantic mutation inventory changed for {pattern.pattern}: "
            f"got {[str(p.relative_to(root)) for p in sorted(hits)]}, "
            f"expected {[str(p.relative_to(root)) for p in sorted(allowed)]}"
        )

exact_mutation_counts = {
    (kernel_hpp, "->flags |= Clause::GARBAGE"): 1,
    # Ordinary clauses plus the detached and lazy native-reason records.
    (kernel_cpp, "->size ="): 4,
    # Ordinary large clauses, detached proof reasons, proof-invisible lazy
    # handles, and in-place replacements are the kernel-owned assignments.
    (kernel_cpp, "->set_id("): 4,
    (parse_cpp, "->set_id("): 2,
}
for (path, token), expected in exact_mutation_counts.items():
    observed = path.read_text().count(token)
    if observed != expected:
        fail(
            f"authoritative mutation count changed for "
            f"{path.relative_to(root)} {token!r}: {observed} != {expected}"
        )

# Checker-population authority. The wrapper-only derived-unit/empty endpoints
# must scope the role and funnel through add_rup; every serializer endpoint
# emits its own typed event.
for signature in (
    "void Proof::flush_pending_dsr_units()",
    "void Proof::add_rup(",
    "void Proof::add_rat(",
    "bool Proof::add_sr(",
    "void Proof::del(",
    "bool Proof::add_external_rup(",
    "bool Proof::del_external(",
    "FILE *Proof::begin_pb()",
    "bool Proof::open(",
):
    require(proof_cpp, signature, "observe_checker(")
require(proof_cpp, "bool Proof::settle_population_observer()",
        "streaming_pbp() && pbp_output_", "flush()")
require(proof_cpp, "bool Proof::open(",
        "CheckerPopulationEventKind::RouteOpened")
require(proof_cpp, "void Proof::bind_population_observer(",
        "ObserverUnboundIncomplete", "population_observer_->observe")
require(proof_cpp, "void Proof::del(",
        "CheckerPopulationEventKind::Delete",
        "CheckerPopulationEventKind::DeleteRequested",
        "CheckerPopulationEventKind::StageDeleteRequested")
require(proof_cpp, "bool Proof::del_external(",
        "CheckerPopulationEventKind::DeleteRequested",
        "CheckerPopulationEventKind::StageDeleteRequested")
for signature in ("void Proof::add_empty(", "void Proof::add_derived_unit("):
    require(proof_cpp, signature, "population_role_", "add_rup(")
require(proof_cpp, "bool Proof::add_external_rat(", "population_role_",
        "add_external_rup(")
require(proof_cpp, "void Proof::seed_population(", "observe_checker(")

# Solver-population authority. Physical mirror publication, arena collection,
# watch pose, and variable compaction are intentionally absent: they do not
# change the logical population.
for signature in (
    "Ref kernel_add_large_clause(",
    "static void add_binary_clause(",
    "void kernel_publish_binary_clause(",
    "bool kernel_delete_binary(",
    "bool kernel_delete_binary_one_copy(",
    "uint32_t kernel_remove_duplicate_binary_copies(",
    "static bool delete_binary_all_copies(",
    "void kernel_materialize_dsr_root_unit(",
):
    require(kernel_cpp, signature, "kernel_observe_solver_clause(")
mirror_body = function_body(kernel_cpp, "void kernel_add_binary_mirror(")
if "kernel_observe_solver_clause(" in mirror_body:
    fail("mirror-only binary mechanics must not emit population events")

# These are all direct binary-watch insertions in production. The kernel
# primitives publish three ordinary/mirror forms; Substitute reconstructs
# directions in place and must finish each logical pair through the audited
# publication primitive. A new direct insertion site requires a fresh audit.
binary_pushes = {
    kernel_cpp: 4,
    src / "rules" / "substitute" / "substitute.cpp": 3,
}
observed_pushes = {
    path: path.read_text().count(".push_binary(")
    for path in src.rglob("*.cpp")
    if ".push_binary(" in path.read_text()
}
if observed_pushes != binary_pushes:
    fail(
        "direct binary membership inventory changed: got "
        f"{[(str(p.relative_to(root)), n) for p, n in observed_pushes.items()]}, "
        "expected "
        f"{[(str(p.relative_to(root)), n) for p, n in binary_pushes.items()]}"
    )

substitute_cpp = src / "rules" / "substitute" / "substitute.cpp"
substitute_text = substitute_cpp.read_text()
publish_calls = re.findall(
    r"kernel_publish_binary_clause\s*\([^;]*?\);", substitute_text, re.S
)
if len(publish_calls) != 2 or any(
    "BinaryPopulationEffect::" not in call and "population_effect" not in call
    for call in publish_calls
):
    fail("every Substitute binary publication must classify its population effect")
require(substitute_cpp, "ApplyResult do_substitution(",
        "retire_source", "SolverPopulationEventKind::StageDelete",
        "SolverPopulationEventKind::StageAdd",
        "BinaryPopulationEffect::RepresentationOnly")

# Substitute is the sole rule allowed to mutate binary watches directly. Its
# two bulk-compaction sites and one single-pass head resize are covered by the
# explicit source-retirement events above. Any new rule-side mutation must
# either use a kernel primitive or extend this audited classification.
rule_binary_mutations = []
for path in (src / "rules").rglob("*"):
    if path.suffix not in {".cpp", ".hpp"}:
        continue
    text = path.read_text()
    count = (text.count(".watches.compact(") +
             text.count(".watches.resize_live(") + len(
        re.findall(r"\bhead\.size\s*=(?!=)", text)
    ))
    if count:
        rule_binary_mutations.append((path, count))
if rule_binary_mutations != [(substitute_cpp, 3)]:
    fail(
        "unclassified rule-side binary membership mutation: "
        f"{[(str(p.relative_to(root)), n) for p, n in rule_binary_mutations]}"
    )
require(kernel_hpp, "inline void kernel_delete_clause(",
        "kernel_observe_solver_clause(")
require(kernel_hpp, "inline void kernel_assign_unit(",
        "kernel_observe_solver_clause(")
require(kernel_cpp, "ReplaceOutcome kernel_replace_clause(",
        "kernel_observe_replacement(", "kernel_delete_clause(",
        "kernel_add_binary_clause(", "kernel_add_large_clause(")
require(kernel_cpp, "void kernel_replace_large_clause_in_place(",
        "kernel_observe_replacement(", "kernel_observe_solver_clause(",
        "kernel_delete_clause(")
require(kernel_cpp, "void kernel_promote_large_irredundant(",
        "SolverPopulationEventKind::MetadataUpdate")

# Raw clauses are captured before the generic and fast-binary normalization
# branches; completion and replay/rebind are explicit.
require(parse_cpp, "static void ingest_clause_generic(",
        "RawInputObservation observation")
require(parse_cpp, "void ingest_clause(", "RawInputObservation observation")
require(parse_cpp, "class RawInputObservation",
        "normalized_available", "PopulationSourceClassification::Eligible",
        "PopulationSourceClassification::Ineligible")
require(parse_cpp, "void finish_input(", "RawInputEventKind::Finished")
require(parse_cpp, "void attach_replayed_input_proof(",
        "observe_checker_id_rebind", "kernel_sync_population_observer")

# Substitute owns one explicit logical transaction around either physical
# binary-rewrite implementation. Every logical delta is staged and every
# direct pair publication is representation-only until the commit callback.
require(substitute_cpp, "ApplyResult do_substitution(",
        "kernel_begin_solver_population_transaction",
        "SolverPopulationEventKind::StageAdd",
        "SolverPopulationEventKind::StageDelete",
        "const bool kissat_single_pass = !session && !lrat",
        "deferred_single_pass_deltas",
        "deferred_population_adds",
        "kernel_commit_solver_population_transaction")
for call in publish_calls:
    if "BinaryPopulationEffect::RepresentationOnly" not in call:
        fail("Substitute publication claimed logical liveness before commit")

print("population observer hook inventory: ok")
