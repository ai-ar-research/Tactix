"""Bounded aggregate of persistent affine source sets from node-local provenance.

The raw structural journal is intentionally never copied to a campaign result.
It includes large proof clauses; this module retains only bounded overlap and
proof-shape aggregates.
"""

from collections import Counter
import hashlib
import json
from pathlib import Path


SCHEMA = "tactix-matrix-source-overlap-v1"
MAXIMUM_BYTES = 256 << 20


def _prefix(left, right):
    count = 0
    for a, b in zip(left, right):
        if a != b:
            break
        count += 1
    return count


def _intersection(left, right):
    a = b = result = 0
    while a < len(left) and b < len(right):
        if left[a] == right[b]:
            result += 1
            a += 1
            b += 1
        elif left[a] < right[b]:
            a += 1
        else:
            b += 1
    return result


def _row_signature(row, sources):
    equations = row.get("sources")
    if not isinstance(equations, list) or len(equations) != len(sources):
        raise ValueError("matrix reason lacks exact source equations")
    variables = set()
    rhs = 0
    for ordinal, equation in zip(sources, equations):
        member = equation.get("variables")
        sign = equation.get("rhs")
        if (equation.get("ordinal") != ordinal or not isinstance(member, list) or
                not member or any(type(x) is not int or x <= 0 for x in member) or
                member != sorted(set(member)) or
                type(sign) is not int or sign not in (0, 1)):
            raise ValueError("invalid matrix source equation")
        variables.symmetric_difference_update(member)
        rhs ^= sign
    return tuple(sorted(variables)), rhs


def summarize(path, maximum_bytes, input_sha256):
    path = Path(path)
    size = path.stat().st_size
    if not 0 < size <= maximum_bytes <= MAXIMUM_BYTES:
        raise ValueError("matrix provenance missing or exceeds declared cap")
    digest = hashlib.sha256()
    prior = []
    cache = {}
    rows_seen = {}
    totals = Counter()
    source_widths = Counter()
    prefix_widths = Counter()
    addition_size_buckets = Counter()
    additions_by_source_width = Counter()
    session = False
    with path.open("rb") as stream:
        for number, line in enumerate(stream, 1):
            digest.update(line)
            if not line.endswith(b"\n"):
                raise ValueError(f"unterminated provenance record {number}")
            row = json.loads(line)
            if number == 1:
                if (row.get("kind") != "session" or row.get("version") != 1 or
                        row.get("input_file_sha256") != input_sha256):
                    raise ValueError("matrix provenance session/input mismatch")
                session = True
                continue
            kind = row.get("kind")
            if kind == "affine-matrix-session":
                totals["matrix_sessions"] += 1
                continue
            if kind != "affine-matrix-reason":
                continue
            proof_cache = row.get("proof_cache", {})
            status = proof_cache.get("status")
            if status == "hit":
                reference = row.get("sources_ref", {})
                cached = cache.get(reference.get("cache_entry"))
                if cached is None:
                    raise ValueError("matrix cache hit lacks earlier source set")
                sources, signature = cached
                totals["cache_hits"] += 1
            else:
                raw = row.get("source_ordinals")
                if (not isinstance(raw, list) or not raw or
                        any(type(x) is not int or x < 0 for x in raw) or
                        raw != sorted(set(raw))):
                    raise ValueError("invalid matrix source ordinals")
                sources = tuple(raw)
                signature = _row_signature(row, sources)
                if status == "insert":
                    entry = proof_cache.get("entry_id")
                    if (type(entry) is not int or entry <= 0 or
                            (entry in cache and cache[entry] != (sources, signature))):
                        raise ValueError("invalid matrix cache entry")
                    cache[entry] = (sources, signature)
                    totals["cache_inserts"] += 1
                elif status != "disabled":
                    raise ValueError("invalid matrix cache status")
            events = row.get("proof_events")
            proof_mode = row.get("proof_mode", "clausal-parity-tree")
            if proof_mode not in ("clausal-parity-tree", "pbp-polynomial"):
                raise ValueError("invalid matrix proof mode")
            if proof_mode == "pbp-polynomial":
                additions = row.get("proof_additions")
                deletions = row.get("proof_deletions")
                if (type(additions) is not int or additions <= 0 or
                        deletions != 0 or
                        (events is not None and events != [])):
                    raise ValueError("invalid polynomial matrix proof counts")
                rat = rup = added_literals = 0
                totals["pbp_polynomial_constraints"] += additions
                events = []
            elif not isinstance(events, list):
                raise ValueError("matrix reason omitted proof events")
            rat = rup = deletions = added_literals = 0
            for event in events:
                if not isinstance(event, dict):
                    raise ValueError("invalid matrix proof event")
                kind = event.get("kind")
                if kind not in ("rat", "rup", "delete"):
                    raise ValueError("invalid matrix proof event kind")
                clause = event.get("clause")
                if not isinstance(clause, list) or any(type(literal) is not int or
                        literal == 0 for literal in clause):
                    raise ValueError("invalid matrix proof clause")
                if kind == "rat":
                    rat += 1
                    added_literals += len(clause)
                elif kind == "rup":
                    rup += 1
                    added_literals += len(clause)
                else:
                    deletions += 1
            if proof_mode != "pbp-polynomial":
                additions = rat + rup
                if (row.get("proof_additions") is not None and
                        row.get("proof_additions") != additions):
                    raise ValueError("matrix proof addition count mismatch")
                if (row.get("proof_deletions") is not None and
                        row.get("proof_deletions") != deletions):
                    raise ValueError("matrix proof deletion count mismatch")
            totals["reasons"] += 1
            totals["sources"] += len(sources)
            totals["proof_additions"] += additions
            totals["proof_deletions"] += deletions
            totals["rat_additions"] += rat
            totals["rup_additions"] += rup
            totals["added_clause_literals"] += added_literals
            totals["maximum_reason_additions"] = max(
                totals["maximum_reason_additions"], additions)
            addition_size_buckets[additions.bit_length()] += 1
            additions_by_source_width[len(sources)] += additions
            if signature in rows_seen and rows_seen[signature] != sources:
                totals["same_row_different_sources"] += 1
            else:
                rows_seen.setdefault(signature, sources)
            source_widths[len(sources)] += 1
            longest = max((_prefix(sources, old) for old in prior), default=0)
            overlap = 0
            subset = 0
            best_delta = None
            best_delta_prior = 0
            for old in prior:
                shared = _intersection(sources, old)
                overlap = max(overlap, shared)
                if shared == len(old):
                    subset = max(subset, len(old))
                delta = len(sources) + len(old) - 2 * shared
                if best_delta is None or delta < best_delta:
                    best_delta = delta
                    best_delta_prior = len(old)
            totals["best_prefix_sources"] += longest
            totals["best_intersection_sources"] += overlap
            totals["best_prior_subset_sources"] += subset
            totals["reasons_with_prior_subset"] += bool(subset)
            if best_delta is not None:
                totals["nearest_prior_delta_sources"] += best_delta
                totals["nearest_prior_sources"] += best_delta_prior
                totals["reasons_with_prior"] += 1
            prefix_widths[longest] += 1
            if sources in prior:
                totals["exact_repeat"] += 1
            prior.append(sources)
    if not session:
        raise ValueError("missing matrix provenance session")
    return {
        "schema": SCHEMA,
        "raw_bytes_deleted_after_collection": size,
        "raw_sha256": digest.hexdigest(),
        "matrix_sessions": totals["matrix_sessions"],
        "reasons": totals["reasons"],
        "distinct_source_sets": len(set(prior)),
        "distinct_row_signatures": len(rows_seen),
        "same_row_different_sources": totals["same_row_different_sources"],
        "cache_hits": totals["cache_hits"],
        "cache_inserts": totals["cache_inserts"],
        "exact_repeat": totals["exact_repeat"],
        "total_sources": totals["sources"],
        "best_prefix_sources": totals["best_prefix_sources"],
        "best_intersection_sources": totals["best_intersection_sources"],
        "best_prior_subset_sources": totals["best_prior_subset_sources"],
        "reasons_with_prior_subset": totals["reasons_with_prior_subset"],
        "nearest_prior_delta_sources": totals["nearest_prior_delta_sources"],
        "nearest_prior_sources": totals["nearest_prior_sources"],
        "reasons_with_prior": totals["reasons_with_prior"],
        "source_widths": dict(sorted(source_widths.items())),
        "best_prefix_widths": dict(sorted(prefix_widths.items())),
        "proof_additions": totals["proof_additions"],
        "proof_deletions": totals["proof_deletions"],
        "rat_additions": totals["rat_additions"],
        "rup_additions": totals["rup_additions"],
        "pbp_polynomial_constraints": totals["pbp_polynomial_constraints"],
        "added_clause_literals": totals["added_clause_literals"],
        "maximum_reason_additions": totals["maximum_reason_additions"],
        "addition_size_buckets": dict(sorted(addition_size_buckets.items())),
        "additions_by_source_width": dict(sorted(additions_by_source_width.items())),
    }
