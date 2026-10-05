#!/usr/bin/env python3
"""Parse and score opt-in GF(2) telemetry from experimental Tier-D runs.

The parser deliberately preserves production counter names.  It does not
turn a missing, truncated, or corrupt stream into a zero.  A zero produced by
a complete stream remains useful diagnostic evidence.
"""
import argparse
from collections import Counter
import csv
import gzip
import hashlib
import json
import lzma
from pathlib import Path
import re
import sys

from inspect_policy_feedback import (integer, keys, native, object_pairs,
                                     observation, observation_delta, outcome, reject_constant,
                                     state)


UINT64_MAX = (1 << 64) - 1
MAX_FEEDBACK_BYTES = 512 << 20
MAX_FEEDBACK_DECODED_BYTES = 8 << 30
MAX_FEEDBACK_RECORDS = 20_000_000
MAX_PROVENANCE_BYTES = 256 << 20
LEGACY_SCHEMA = "tactix-experimental-gf2-telemetry-v1"
SCHEMA = "tactix-experimental-gf2-telemetry-v2"
FEEDBACK_SCHEMA = "tactix-experimental-gf2-feedback-summary-v1"
PROVENANCE_SCHEMA = "tactix-experimental-gf2-provenance-summary-v1"

AFFINE_COUNTERS = (
    "affine_consequence_attempts",
    "affine_consequence_applied",
    "affine_consequence_no_structure",
    "affine_consequence_invalid",
    "affine_consequence_inapplicable",
    "affine_consequence_resource_exhausted",
    "affine_consequence_interrupted",
    "affine_consequence_unsupported_proof",
    "affine_consequence_stale",
    "affine_consequence_already_published",
    "affine_consequence_snapshot_clauses",
    "affine_consequence_snapshot_literals",
    "affine_consequence_snapshot_work",
    "affine_consequence_equations",
    "affine_consequence_consequences",
    "affine_consequence_recognition_work",
    "affine_consequence_selected_sources",
    "affine_consequence_selected_premises",
    # This is retained under its production name.  CertifiedBasisV2 charges
    # its wide derivation work to affine_consequence_basis_work instead.
    "affine_consequence_proof_work",
    "affine_consequence_preflight_additions",
    "affine_consequence_preflight_deletions",
    "affine_consequence_additions",
    "affine_consequence_deletions",
    "affine_consequence_published",
    "affine_consequence_journal_work",
    "affine_consequence_journal_bytes",
    "affine_consequence_candidate_variables",
    "affine_consequence_basis_work",
    "affine_consequence_rows",
    "affine_consequence_definitions",
    "affine_consequence_extent_growth",
    "affine_consequence_session_work",
    "affine_consequence_excluded_rows",
    "affine_consequence_transaction_keys",
    "affine_consequence_committed_keys",
)

PRODUCER_RE = re.compile(
    r"^c controller-producer AffineConsequence calls=(\d+) work=(\d+) "
    r"candidates=(\d+) offers=(\d+)$")
ACTION_RE = re.compile(
    r"^c controller-rule AffineConsequence requests=(\d+) applications=(\d+) "
    r"effective=(\d+) exhausted=(\d+) stale=(\d+) interrupted=(\d+) "
    r"work=(\d+) overshoot=(\d+)$")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def xz_content_sha256(path):
    digest = hashlib.sha256()
    with lzma.open(path, "rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def _json(line, line_number):
    try:
        value = json.loads(line, object_pairs_hook=object_pairs,
                           parse_constant=reject_constant)
    except (ValueError, TypeError) as error:
        raise ValueError(f"line {line_number}: {error}") from error
    require(type(value) is dict, f"line {line_number}: record is not an object")
    return value


class BoundedJsonl:
    def __init__(self, path, stored_max_bytes, decoded_max_bytes, maximum_records):
        self.path = Path(path)
        self.stored_bytes = self.path.stat().st_size
        require(type(stored_max_bytes) is int and 0 < stored_max_bytes <= UINT64_MAX,
                "invalid JSONL stored byte cap")
        require(type(decoded_max_bytes) is int and 0 < decoded_max_bytes <= UINT64_MAX,
                "invalid JSONL decoded byte cap")
        require(type(maximum_records) is int and 0 < maximum_records <= UINT64_MAX,
                "invalid JSONL record cap")
        require(self.stored_bytes <= stored_max_bytes, "JSONL stored byte cap exceeded")
        with self.path.open("rb") as source:
            self.compression = "gzip" if source.read(2) == b"\x1f\x8b" else "none"
        self.decoded_max_bytes = decoded_max_bytes
        self.maximum_records = maximum_records
        self.decoded_bytes = 0
        self.records = 0
        self.terminated = True
        self.logical_sha256 = hashlib.sha256()

    def __iter__(self):
        with self.path.open("rb") as raw:
            source = gzip.GzipFile(fileobj=raw) if self.compression == "gzip" else raw
            try:
                for number, line in enumerate(source, 1):
                    self.decoded_bytes += len(line)
                    require(self.decoded_bytes <= self.decoded_max_bytes,
                            "JSONL decoded byte cap exceeded")
                    self.logical_sha256.update(line)
                    if not line.endswith(b"\n"):
                        self.terminated = False
                        break
                    require(line != b"\n", f"line {number}: empty JSONL record")
                    self.records += 1
                    require(self.records <= self.maximum_records,
                            "JSONL record cap exceeded")
                    try:
                        text = line.decode("utf-8")
                    except UnicodeDecodeError as error:
                        raise ValueError("JSONL is not UTF-8") from error
                    yield _json(text, number)
            except (EOFError, gzip.BadGzipFile, OSError) as error:
                raise ValueError("invalid/truncated gzip JSONL") from error


def bounded_jsonl(path, maximum_bytes):
    """Return complete JSON records and whether the byte stream was complete.

    A killed process may leave one non-newline-terminated record.  That final
    fragment is retained in the hashed sidecar but cannot become evidence.
    Every complete JSONL record remains strict, including duplicate-key and
    non-finite-number rejection.
    """
    data = Path(path).read_bytes()
    require(len(data) <= maximum_bytes, "JSONL byte cap exceeded")
    stream = BoundedJsonl(path, maximum_bytes, maximum_bytes, UINT64_MAX)
    records = list(stream)
    return data, records, stream.terminated


def parse_feedback_metadata(metadata):
    require(type(metadata) is str, "invalid feedback metadata")
    prefix = "tactix-frontend-feedback-v1\n"
    require(metadata.startswith(prefix), "unsupported feedback metadata")
    fields = {}
    cursor = len(prefix)
    while cursor < len(metadata):
        equals = metadata.find("=", cursor)
        colon = metadata.find(":", equals + 1)
        require(equals > cursor and colon > equals + 1,
                "malformed feedback metadata field")
        name = metadata[cursor:equals]
        require(name not in fields, "duplicate feedback metadata field")
        length_text = metadata[equals + 1:colon]
        require(length_text.isdigit(), "invalid feedback metadata length")
        length = int(length_text)
        start, end = colon + 1, colon + 1 + length
        require(end < len(metadata) and metadata[end] == "\n",
                "truncated feedback metadata field")
        fields[name] = metadata[start:end]
        cursor = end + 1
    return fields


def _validate_request(value):
    if value is None:
        return []
    keys(value, "version sequence type expected search search_components invocation",
         "request", optional="producer_effort")
    integer(value["version"], "request version", (1 << 32) - 1)
    integer(value["sequence"], "request sequence")
    integer(value["type"], "request type", 255)
    state(value["expected"])
    require(type(value["search"]) is str and type(value["invocation"]) is str,
            "invalid request payload")
    require(type(value["search_components"]) is list and
            len(value["search_components"]) == 3,
            "invalid search components")
    for component in value["search_components"]:
        integer(component, "raw search component", 255)
    producers = value.get("producer_effort", [])
    require(type(producers) is list, "invalid producer effort")
    for producer in producers:
        keys(producer, "family status complete preparation_work enumeration_work "
             "total_work candidates offers", "producer effort")
        require(type(producer["family"]) is str and producer["family"],
                "invalid producer family")
        integer(producer["status"], "producer status", 255)
        require(type(producer["complete"]) is bool, "invalid producer completeness")
        for name in ("preparation_work", "enumeration_work", "total_work",
                     "candidates", "offers"):
            integer(producer[name], "producer " + name)
        require(producer["total_work"] == min(
            UINT64_MAX, producer["preparation_work"] + producer["enumeration_work"]),
            "producer total work mismatch")
    return producers


def _validate_allowance(value):
    if value is None:
        return
    keys(value, "unit amount native_mode native_amount", "allowance")
    for name, item in value.items():
        integer(item, name)
    integer(value["unit"], "raw allowance unit", 255)
    integer(value["native_mode"], "raw native budget mode", 255)


def _validate_native_request(value):
    keys(value, "version description invocation currency unlimited amount work "
         "overshoot accounting_overflow", "native request")
    require(integer(value["version"], "native request version") == 1,
            "unsupported native request version")
    for name in ("description", "currency"):
        require(type(value[name]) is str, f"invalid native {name}")
    require(value["currency"], "missing native currency")
    require(value["invocation"] is None or type(value["invocation"]) is str,
            "invalid native invocation")
    for name in ("unlimited", "accounting_overflow"):
        require(type(value[name]) is bool, f"invalid native {name}")
    for name in ("amount", "work", "overshoot"):
        integer(value[name], f"native {name}")
    require(not value["unlimited"] or value["amount"] == 0,
            "invalid unlimited native allowance")
    require(value["overshoot"] == (0 if value["unlimited"] else
            max(value["work"] - value["amount"], 0)),
            "native overshoot mismatch")
    require(not value["accounting_overflow"] or value["work"] == 0,
            "invalid overflow measurement")


def _affine_deltas(before, after):
    result = {}
    for name in AFFINE_COUNTERS:
        require(name in before and name in after, f"missing native counter {name}")
        left = integer(before[name], name)
        right = integer(after[name], name)
        require(right >= left, f"non-monotone native counter {name}")
        result[name] = right - left
    return result


def parse_feedback(path, maximum_bytes, maximum_decoded_bytes=None,
                   maximum_records=None):
    maximum_decoded_bytes = maximum_decoded_bytes or maximum_bytes
    maximum_records = maximum_records or UINT64_MAX
    records = BoundedJsonl(path, maximum_bytes, maximum_decoded_bytes,
                           maximum_records)
    header = None
    wire_version = None
    current_observation = None
    begun = ended = False
    pending = None
    action_sequence = interval_sequence = 0
    actions = dict(records=0, applications=0, effective=0, work=0,
                   statuses=Counter(), native={name: 0 for name in AFFINE_COUNTERS})
    bookkeeping = {name: dict(records=0, applications=0, effective=0, work=0,
                              native=Counter())
                   for name in ("Propagate", "SemanticVariableAdvice")}
    producer = dict(calls=0, complete=0, preparation_work=0,
                    enumeration_work=0, total_work=0, candidates=0, offers=0,
                    statuses=Counter())
    terminal = None
    for number, row in enumerate(records, 1):
        try:
            kind = row.get("type")
            require(not ended, "record after session end")
            if pending is not None:
                keys(row, "type value", "elapsed record")
                require(kind == "elapsed_nanos", "missing elapsed record")
                integer(row["value"], "elapsed nanos")
                pending = None
                continue
            if header is None:
                keys(row, "type version metadata", "header")
                require(kind == "header", "unsupported header")
                wire_version = integer(row["version"], "header version")
                require(wire_version in (1, 2), "unsupported header")
                header = row
                continue
            if not begun:
                keys(row, "type observation", "session begin")
                require(kind == "session_begin", "missing session begin")
                observation(row["observation"])
                current_observation = row["observation"]
                begun = True
                continue
            if kind == "session_end":
                keys(row, ("type verdict stop observation" if wire_version == 1 else
                           "type verdict stop observation_delta"), "session end")
                require(type(row["verdict"]) is int and row["verdict"] in (-1, 0, 10, 20),
                        "invalid verdict")
                integer(row["stop"], "stop reason", 18)
                if wire_version == 1:
                    observation(row["observation"])
                    current_observation = row["observation"]
                else:
                    current_observation = observation_delta(
                        current_observation, row["observation_delta"])
                terminal = {"verdict": row["verdict"], "stop": row["stop"]}
                ended = True
                continue
            require(kind in ("action", "interval"), "unexpected record type")
            require(integer(row.get("version"), "record version") == wire_version,
                    "unsupported record version")
            if wire_version == 1:
                observation(row["before"])
                observation(row["after"])
            else:
                row["before"] = observation_delta(current_observation,
                                                    row["before_delta"])
                row["after"] = observation_delta(row["before"], row["after_delta"])
                current_observation = row["after"]
            integer(row["stop"], "stop reason", 18)
            if kind == "interval":
                keys(row, ("type version sequence preceding start stop before after"
                           if wire_version == 1 else
                           "type version sequence preceding start stop before_delta after_delta"),
                     "interval", optional="before after" if wire_version == 2 else "")
                interval_sequence += 1
                require(integer(row["sequence"], "interval sequence") == interval_sequence,
                        "interval sequence gap")
                integer(row["start"], "interval start", 2)
                integer(row["preceding"], "preceding interval")
                require(row["preceding"] == (interval_sequence - 1 if row["start"] else 0)
                        and (not row["start"] or interval_sequence > 1),
                        "invalid continuation link")
                pending = kind
                continue
            keys(row, ("type version sequence kind identity instance request allowance "
                       "before after outcome stop" if wire_version == 1 else
                       "type version sequence kind identity instance request allowance "
                       "before_delta after_delta outcome stop"), "action",
                 optional="native_request before after" if wire_version == 2 else
                          "native_request")
            action_sequence += 1
            require(integer(row["sequence"], "action sequence") == action_sequence,
                    "action sequence gap")
            integer(row["kind"], "action kind", 3)
            integer(row["instance"], "instance")
            require(type(row["identity"]) is str and row["identity"],
                    "missing action identity")
            producers = _validate_request(row["request"])
            _validate_allowance(row["allowance"])
            outcome(row["outcome"])
            require(row["outcome"]["before"] == row["before"]["state"] and
                    row["outcome"]["after"] == row["after"]["state"],
                    "outcome state mismatch")
            if "native_request" in row:
                _validate_native_request(row["native_request"])
            for effort in producers:
                if effort["family"] != "AffineConsequence":
                    continue
                producer["calls"] += 1
                producer["complete"] += int(effort["complete"])
                producer["statuses"][str(effort["status"])] += 1
                for name in ("preparation_work", "enumeration_work", "total_work",
                             "candidates", "offers"):
                    producer[name] += effort[name]
            if row["identity"] == "AffineConsequence":
                actions["records"] += 1
                actions["applications"] += row["outcome"]["applications"]
                actions["effective"] += int(row["outcome"]["progress"])
                actions["work"] += (row["native_request"]["work"]
                                    if "native_request" in row
                                    else row["outcome"]["work"])
                actions["statuses"][str(row["outcome"]["status"])] += 1
                deltas = _affine_deltas(row["before"]["native"],
                                        row["after"]["native"])
                for name, amount in deltas.items():
                    actions["native"][name] += amount
            if row["identity"] in bookkeeping:
                entry = bookkeeping[row["identity"]]
                entry["records"] += 1
                entry["applications"] += row["outcome"]["applications"]
                entry["effective"] += int(row["outcome"]["progress"])
                entry["work"] += (row["native_request"]["work"]
                                  if "native_request" in row else
                                  row["outcome"]["work"])
                for name, after_value in row["after"]["native"].items():
                    before_value = row["before"]["native"].get(name)
                    if (type(after_value) is int and type(before_value) is int and
                            after_value >= before_value and after_value != before_value):
                        entry["native"][name] += after_value - before_value
            pending = kind
        except (KeyError, TypeError, ValueError, OverflowError) as error:
            raise ValueError(f"line {number}: {error}") from error
    complete = bool(records.terminated and ended and pending is None)
    metadata = parse_feedback_metadata(header["metadata"]) if header else None
    actions["statuses"] = dict(sorted(actions["statuses"].items()))
    producer["statuses"] = dict(sorted(producer["statuses"].items()))
    for entry in bookkeeping.values():
        entry["native"] = dict(sorted(entry["native"].items()))
    result = dict(schema=FEEDBACK_SCHEMA, bytes=records.decoded_bytes,
                  lines=records.records, complete=complete, metadata=metadata,
                  terminal=terminal, producer=producer, action=actions)
    if wire_version == 2:
        result.update(wire_version=wire_version, compression=records.compression,
                      stored_bytes=records.stored_bytes,
                      logical_sha256=records.logical_sha256.hexdigest(),
                      bookkeeping=bookkeeping)
    return result


def _uint(value, name):
    return integer(value, name)


def _int_list(value, name, *, nonzero=False):
    require(type(value) is list, f"invalid {name}")
    for item in value:
        require(type(item) is int and (not nonzero or item != 0), f"invalid {name}")


def _validate_provenance_row(row):
    kind = row.get("kind")
    common = "action semantic owner kind"
    if kind == "affine-basis-begin":
        keys(row, common + " equations rows definitions propagation", "basis begin")
        for name in ("action", "semantic", "owner", "equations", "rows", "definitions"):
            _uint(row[name], name)
        require(row["propagation"] == "deferred-controller", "invalid propagation contract")
    elif kind == "affine-basis-source":
        keys(row, common + " occurrence source_id clause", "basis source")
        for name in ("action", "semantic", "owner", "occurrence", "source_id"):
            _uint(row[name], name)
        _int_list(row["clause"], "source clause", nonzero=True)
    elif kind == "affine-basis-row":
        keys(row, common + " row rhs variables sources premises", "basis row")
        for name in ("action", "semantic", "owner", "row"):
            _uint(row[name], name)
        require(type(row["rhs"]) is bool, "invalid row rhs")
        for name in ("variables", "sources", "premises"):
            _int_list(row[name], name)
    elif kind in ("affine-basis-definition-rat", "affine-basis-helper-fix-rat",
                  "affine-basis-proof-rat", "affine-basis-proof-rup",
                  "affine-basis-proof-delete"):
        keys(row, common + " id install clause", "basis proof event")
        for name in ("action", "semantic", "owner", "id"):
            _uint(row[name], name)
        require(type(row["install"]) is bool, "invalid install flag")
        _int_list(row["clause"], "proof clause", nonzero=True)
    elif kind == "affine-basis-theory-variable":
        keys(row, common + " output meaning first second projection reconstruction",
             "basis theory variable")
        for name in ("action", "semantic", "owner", "output", "first", "second"):
            _uint(row[name], name)
        require(row["meaning"] == "output iff first XOR second" and
                row["projection"] == "omit-extension" and
                row["reconstruction"] == "evaluate-operand-xor",
                "invalid theory-variable semantics")
    elif kind == "affine-basis-proof-helper":
        keys(row, common + " output meaning projection reconstruction", "basis proof helper")
        for name in ("action", "semantic", "owner", "output"):
            _uint(row[name], name)
        require(row["meaning"] == "retired transition identity fixed true" and
                row["projection"] == "omit-extension" and
                row["reconstruction"] == "assign-true",
                "invalid proof-helper semantics")
    elif kind == "affine-basis-complete":
        keys(row, common + " additions deletions published extent_growth", "basis complete")
        for name in ("action", "semantic", "owner", "additions", "deletions",
                     "published", "extent_growth"):
            _uint(row[name], name)
    else:
        raise ValueError(f"unsupported structural provenance kind {kind!r}")


def parse_provenance(path, maximum_bytes):
    data, records, terminated = bounded_jsonl(path, maximum_bytes)
    require(records, "missing structural provenance session")
    session = records[0]
    keys(session, "kind version input_file_sha256", "provenance session")
    require(session["kind"] == "session" and session["version"] == 1,
            "unsupported structural provenance session")
    require(type(session["input_file_sha256"]) is str and
            re.fullmatch(r"[0-9a-f]{64}", session["input_file_sha256"]),
            "invalid provenance input hash")
    totals = Counter(source_records=0, row_records=0, proof_events=0,
                     definition_events=0, helper_fix_events=0,
                     theory_variables=0, proof_helpers=0, additions=0,
                     deletions=0, published_clauses=0, extent_growth=0)
    active = None
    actions = 0
    event_kinds = Counter()
    for number, row in enumerate(records[1:], 2):
        try:
            _validate_provenance_row(row)
            identity = (row["action"], row["semantic"], row["owner"])
            kind = row["kind"]
            if kind == "affine-basis-begin":
                require(active is None, "nested basis transaction")
                active = dict(identity=identity, rows=row["rows"],
                              definitions=row["definitions"], sources=0,
                              row_records=0, theory_variables=0,
                              proof_helpers=0, additions=0, deletions=0,
                              installed=0)
                actions += 1
                continue
            require(active is not None and active["identity"] == identity,
                    "record outside its basis transaction")
            event_kinds[kind] += 1
            if kind == "affine-basis-source":
                active["sources"] += 1
                totals["source_records"] += 1
            elif kind == "affine-basis-row":
                require(row["row"] == active["row_records"], "basis row sequence gap")
                active["row_records"] += 1
                totals["row_records"] += 1
            elif kind in ("affine-basis-definition-rat",
                          "affine-basis-helper-fix-rat",
                          "affine-basis-proof-rat", "affine-basis-proof-rup",
                          "affine-basis-proof-delete"):
                totals["proof_events"] += 1
                if kind == "affine-basis-proof-delete":
                    active["deletions"] += 1
                else:
                    active["additions"] += 1
                    active["installed"] += int(row["install"])
                if kind == "affine-basis-definition-rat":
                    totals["definition_events"] += 1
                if kind == "affine-basis-helper-fix-rat":
                    totals["helper_fix_events"] += 1
            elif kind == "affine-basis-theory-variable":
                active["theory_variables"] += 1
                totals["theory_variables"] += 1
            elif kind == "affine-basis-proof-helper":
                active["proof_helpers"] += 1
                totals["proof_helpers"] += 1
            elif kind == "affine-basis-complete":
                require(active["rows"] == active["row_records"],
                        "basis row count mismatch")
                require(active["definitions"] == active["theory_variables"],
                        "basis definition count mismatch")
                require(row["additions"] == active["additions"] and
                        row["deletions"] == active["deletions"],
                        "basis proof event count mismatch")
                require(row["published"] == active["installed"],
                        "basis published count mismatch")
                require(row["extent_growth"] ==
                        active["theory_variables"] + active["proof_helpers"],
                        "basis extent count mismatch")
                totals["additions"] += row["additions"]
                totals["deletions"] += row["deletions"]
                totals["published_clauses"] += row["published"]
                totals["extent_growth"] += row["extent_growth"]
                active = None
        except (KeyError, TypeError, ValueError, OverflowError) as error:
            raise ValueError(f"line {number}: {error}") from error
    return dict(schema=PROVENANCE_SCHEMA, bytes=len(data), lines=len(records),
                complete=bool(terminated and active is None),
                input_file_sha256=session["input_file_sha256"], actions=actions,
                counts=dict(totals), event_kinds=dict(sorted(event_kinds.items())))


def parse_controller_statistics(lines):
    require(type(lines) is list and all(type(line) is str for line in lines),
            "invalid solver statistics")
    producer = None
    action = None
    for line in lines:
        match = PRODUCER_RE.fullmatch(line)
        if match:
            require(producer is None, "duplicate GF2 producer totals")
            producer = dict(zip(("calls", "work", "candidates", "offers"),
                                map(int, match.groups())))
        match = ACTION_RE.fullmatch(line)
        if match:
            require(action is None, "duplicate GF2 action totals")
            action = dict(zip(("requests", "applications", "effective", "exhausted",
                               "stale", "interrupted", "work", "overshoot"),
                              map(int, match.groups())))
    return {"producer": producer or dict(calls=0, work=0, candidates=0, offers=0),
            "action": action or dict(requests=0, applications=0, effective=0,
                                     exhausted=0, stale=0, interrupted=0,
                                     work=0, overshoot=0)}


def raw_yield(feedback, provenance, solver_statistics):
    coverage = parse_controller_statistics(solver_statistics)
    native_counts = feedback["action"]["native"]
    result = {
        "producer": {**coverage["producer"],
                     "statuses": feedback["producer"]["statuses"],
                     "complete": feedback["producer"]["complete"],
                     "preparation_work": feedback["producer"]["preparation_work"],
                     "enumeration_work": feedback["producer"]["enumeration_work"]},
        "action": coverage["action"],
        "status": {name: native_counts[name] for name in AFFINE_COUNTERS[:10]},
        "work": {name: native_counts[name] for name in (
            "affine_consequence_snapshot_work",
            "affine_consequence_recognition_work",
            "affine_consequence_proof_work",
            "affine_consequence_basis_work",
            "affine_consequence_journal_work",
            "affine_consequence_session_work")},
        "preflight_and_actual": {name: native_counts[name] for name in (
            "affine_consequence_preflight_additions",
            "affine_consequence_preflight_deletions",
            "affine_consequence_additions",
            "affine_consequence_deletions")},
        "input_and_structure": {name: native_counts[name] for name in (
            "affine_consequence_snapshot_clauses",
            "affine_consequence_snapshot_literals",
            "affine_consequence_equations",
            "affine_consequence_consequences",
            "affine_consequence_selected_sources",
            "affine_consequence_selected_premises",
            "affine_consequence_journal_bytes",
            "affine_consequence_excluded_rows")},
        "extent_and_yield": {name: native_counts[name] for name in (
            "affine_consequence_rows", "affine_consequence_definitions",
            "affine_consequence_extent_growth", "affine_consequence_published",
            "affine_consequence_candidate_variables",
            "affine_consequence_transaction_keys",
            "affine_consequence_committed_keys")},
        "structural": provenance["counts"],
    }
    if "bookkeeping" in feedback:
        result["bookkeeping"] = feedback["bookkeeping"]
    return result


def sidecar_descriptor(path, campaign, logical=None):
    path = Path(path)
    if logical is None:
        return {"path": str(path.relative_to(campaign)),
                "bytes": path.stat().st_size, "sha256": sha256(path)}
    return {"path": str(path.relative_to(campaign)),
            "stored_bytes": path.stat().st_size, "stored_sha256": sha256(path),
            "logical_bytes": logical["bytes"],
            "logical_sha256": logical["logical_sha256"],
            "records": logical["lines"]}


def _validate_modern_feedback(feedback):
    require(feedback["wire_version"] == 2, "feedback wire version mismatch")
    require(feedback["compression"] == "gzip", "feedback compression mismatch")


def _verify_logical_descriptor(descriptor, feedback):
    require(descriptor["logical_bytes"] == feedback["bytes"] and
            descriptor["logical_sha256"] == feedback["logical_sha256"] and
            descriptor["records"] == feedback["lines"],
            "feedback logical descriptor mismatch")


def expected_checker_hashes(manifest, arm):
    names = {"checkmodel", "modelcheck.py"}
    certificate = arm.get("certificate", "drat")
    if certificate == "drat":
        names.add("drat-trim")
    elif certificate in ("dsr", "auto"):
        names.update({"dsr-trim", "dsr-compress", "srcheck", "drat-trim",
                      "cake_lpr"})
        if certificate == "auto":
            names.update({"veripb", "cake-pb-cnf"})
    else:
        raise ValueError(f"unsupported certificate mode {certificate}")
    return {"pins/" + name: manifest["pin_sha256"]["pins/" + name]
            for name in sorted(names)}


def validate_arm_contract(campaign, manifest):
    require(manifest.get("schema") ==
            "tactix-experimental-single-tierd-v4-gf2-telemetry",
            "unsupported GF2 campaign schema")
    require(set(manifest.get("arms", {})) == {"tactix"},
            "GF2 campaign requires one tactix arm")
    arm = manifest["arms"]["tactix"]
    require(arm.get("certificate") == "dsr", "GF2 campaign requires DSR")
    arguments = arm.get("arguments")
    require(type(arguments) is list and all(type(arg) is str for arg in arguments),
            "invalid GF2 arm arguments")
    require(sum(arg == "--dsr" for arg in arguments) == 1,
            "GF2 arm requires one --dsr")
    require(sum(arg == "--controller-policy" or
                arg.startswith("--controller-policy=") for arg in arguments) == 1,
            "GF2 arm requires one controller policy")
    require(arguments.count("--controller-families=AffineConsequence") == 1,
            "GF2 arm requires the AffineConsequence family")
    require(arguments.count("--controller-native-producers=gf2-affine") == 1,
            "GF2 arm requires only the gf2-affine native producer")
    forbidden = ("--feedback", "--feedback-replay", "--structural-provenance",
                 "--controller-actions")
    require(not any(arg == name or arg.startswith(name + "=")
                    for arg in arguments for name in forbidden),
            "GF2 arm contains a caller-owned path or static action source")
    profile = f"--controller-gf2-affine-profile={campaign / 'pins/gf2-affine.profile'}"
    require(arguments.count(profile) == 1 and
            "pins/gf2-affine.profile" in manifest.get("pin_sha256", {}),
            "GF2 arm does not use its pinned affine profile")
    return arm


def matrix_rows(campaign):
    with (campaign / "matrix.tsv").open(newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    require(rows and set(rows[0]) ==
            {"task", "rank", "arm", "tier", "instance", "expected"},
            "invalid GF2 matrix schema")
    indexed = {}
    for row in rows:
        task = integer(int(row["task"]), "matrix task")
        require(task not in indexed, "duplicate matrix task")
        require(task == len(indexed), "matrix task sequence gap")
        indexed[task] = row
    return indexed


def independent_checked(result):
    if result.get("verdict") == "SAT":
        check = result.get("model_check") or {}
    elif result.get("verdict") == "UNSAT":
        check = result.get("proof_check") or {}
    else:
        return False
    expected_code = 10 if result["verdict"] == "SAT" else 20
    solve = result.get("solve") or {}
    return bool(result.get("valid") and check.get("result") == "verified" and
                solve.get("exit_code") == expected_code and
                solve.get("timed_out") is False)


def _verify_descriptor(campaign, descriptor, maximum_bytes, logical=False):
    fields = ({"path", "stored_bytes", "stored_sha256", "logical_bytes",
               "logical_sha256", "records"} if logical else
              {"path", "bytes", "sha256"})
    require(type(descriptor) is dict and set(descriptor) == fields,
            "invalid sidecar descriptor")
    path = (campaign / descriptor["path"]).resolve()
    require(path.is_relative_to(campaign.resolve()), "sidecar escapes campaign")
    require(path.is_file(), "missing sidecar")
    size_name, hash_name = (("stored_bytes", "stored_sha256") if logical else
                            ("bytes", "sha256"))
    require(path.stat().st_size == descriptor[size_name] <= maximum_bytes,
            "sidecar size mismatch")
    require(sha256(path) == descriptor[hash_name], "sidecar hash mismatch")
    return path


def _cross_check(feedback, provenance, raw):
    native_counts = feedback["action"]["native"]
    action = raw["action"]
    producer = raw["producer"]
    require(feedback["producer"]["calls"] == producer["calls"],
            "feedback/producer call mismatch")
    require(feedback["producer"]["total_work"] == producer["work"],
            "feedback/producer work mismatch")
    require(feedback["producer"]["candidates"] == producer["candidates"] and
            feedback["producer"]["offers"] == producer["offers"],
            "feedback/producer yield mismatch")
    require(feedback["action"]["records"] == action["requests"],
            "feedback/action request mismatch")
    require(feedback["action"]["applications"] == action["applications"],
            "feedback/action application mismatch")
    require(feedback["action"]["effective"] == action["effective"],
            "feedback/action effect mismatch")
    require(feedback["action"]["work"] == action["work"],
            "feedback/action work mismatch")
    require(native_counts["affine_consequence_attempts"] == action["applications"],
            "native/action attempt mismatch")
    pairs = {
        "affine_consequence_additions": "additions",
        "affine_consequence_deletions": "deletions",
        "affine_consequence_published": "published_clauses",
        "affine_consequence_rows": "row_records",
        "affine_consequence_definitions": "theory_variables",
        "affine_consequence_extent_growth": "extent_growth",
    }
    for native_name, structural_name in pairs.items():
        require(native_counts[native_name] == provenance["counts"][structural_name],
                f"feedback/provenance mismatch for {native_name}")
    require(native_counts["affine_consequence_preflight_additions"] >=
            native_counts["affine_consequence_additions"],
            "actual additions exceed preflight")
    require(native_counts["affine_consequence_preflight_deletions"] >=
            native_counts["affine_consequence_deletions"],
            "actual deletions exceed preflight")


def classify(raw, checked):
    producer, action = raw["producer"], raw["action"]
    if producer["calls"] == 0:
        return "producer-not-called"
    if producer["offers"] == 0:
        return "no-offer"
    if action["applications"] == 0:
        return "offer-not-applied"
    if action["effective"] == 0:
        return "applied-no-effect"
    if not checked:
        return "effective-unchecked"
    return "checked-recovery"


def validate_result(campaign, result_path):
    campaign = Path(campaign).resolve()
    manifest_path = campaign / "manifest.json"
    manifest = json.loads(manifest_path.read_text(), object_pairs_hook=object_pairs,
                          parse_constant=reject_constant)
    contract = manifest.get("gf2_telemetry")
    require(type(contract) is dict and contract.get("schema") in (LEGACY_SCHEMA, SCHEMA),
            "campaign has no GF2 telemetry contract")
    modern = contract["schema"] == SCHEMA
    expected_contract_fields = (
        {"schema", "directory", "feedback_compression", "feedback_wire_version",
         "feedback_stored_max_bytes", "feedback_decoded_max_bytes",
         "feedback_max_records", "structural_provenance_max_bytes"} if modern else
        {"schema", "directory", "feedback_max_bytes",
         "structural_provenance_max_bytes"})
    require(set(contract) == expected_contract_fields and
            contract.get("directory") == "telemetry",
            "invalid campaign telemetry contract fields")
    stored_cap = contract.get("feedback_stored_max_bytes" if modern else
                              "feedback_max_bytes")
    require(type(stored_cap) is int and 0 < stored_cap <= MAX_FEEDBACK_BYTES,
            "invalid campaign feedback stored cap")
    if modern:
        require(contract.get("feedback_compression") == "gzip" and
                contract.get("feedback_wire_version") == 2,
                "invalid campaign feedback format")
        decoded_cap = contract.get("feedback_decoded_max_bytes")
        record_cap = contract.get("feedback_max_records")
        require(type(decoded_cap) is int and
                0 < decoded_cap <= MAX_FEEDBACK_DECODED_BYTES,
                "invalid campaign feedback decoded cap")
        require(type(record_cap) is int and 0 < record_cap <= MAX_FEEDBACK_RECORDS,
                "invalid campaign feedback record cap")
    else:
        decoded_cap, record_cap = stored_cap, UINT64_MAX
    require(type(contract.get("structural_provenance_max_bytes")) is int and
            0 < contract["structural_provenance_max_bytes"] <= MAX_PROVENANCE_BYTES,
            "invalid campaign provenance cap")
    manifest_hash = sha256(manifest_path)
    arm_contract = validate_arm_contract(campaign, manifest)
    launch = json.loads((campaign / "launch.json").read_text(),
                        object_pairs_hook=object_pairs, parse_constant=reject_constant)
    require(launch.get("manifest_sha256") == manifest_hash,
            "launch/manifest hash mismatch")
    result = json.loads(Path(result_path).read_text(), object_pairs_hook=object_pairs,
                        parse_constant=reject_constant)
    require(result.get("schema") == "tactix-experimental-gf2-runtime-result-v1",
            "unsupported GF2 result schema")
    rows = matrix_rows(campaign)
    require(type(result.get("task")) is int and result["task"] in rows,
            "result task is outside matrix")
    matrix = rows[result["task"]]
    expected_identity = {
        "task": int(matrix["task"]), "rank": int(matrix["rank"]),
        "arm": matrix["arm"], "tier": matrix["tier"],
        "instance": matrix["instance"], "expected": matrix["expected"],
    }
    require(all(result.get(name) == value for name, value in expected_identity.items()),
            "result/matrix identity mismatch")
    telemetry = result.get("gf2_telemetry")
    require(type(telemetry) is dict and telemetry.get("schema") == contract["schema"],
            "missing result telemetry")
    require(not telemetry.get("integrity_error"), "runtime telemetry integrity error")
    bindings = telemetry.get("bindings") or {}
    require(bindings.get("manifest_sha256") == manifest_hash,
            "result/manifest hash mismatch")
    for relative, expected in manifest["pin_sha256"].items():
        require(sha256(campaign / relative) == expected, f"pin hash mismatch: {relative}")
    arm = manifest["arms"][result["arm"]]
    require(arm is arm_contract, "result arm is outside GF2 contract")
    require(bindings.get("binary_sha256") == arm["sha256"] ==
            sha256(campaign / arm["path"]), "binary binding mismatch")
    checker_hashes = expected_checker_hashes(manifest, arm)
    require(bindings.get("checker_sha256") == checker_hashes,
            "checker binding mismatch")
    compressed = campaign.parents[2] / "benchmarks/cache" / result["instance"]
    require(bindings.get("compressed_input_sha256") ==
            manifest["input_sha256"][result["instance"]] == sha256(compressed),
            "compressed input binding mismatch")
    require(bindings.get("decompressed_input_sha256") == xz_content_sha256(compressed),
            "decompressed input binding mismatch")
    authority = manifest["dataset_authority"]
    authority_path = Path(authority["path"])
    if not authority_path.is_absolute():
        authority_path = campaign.parents[2] / authority_path
    require(sha256(authority_path) == authority["sha256"],
            "dataset authority mismatch")
    selection = manifest.get("selection_authority")
    if selection:
        require(sha256(campaign.parents[2] / selection["path"]) == selection["sha256"],
                "selection authority mismatch")
    sidecars = telemetry.get("sidecars") or {}
    feedback_path = _verify_descriptor(
        campaign, sidecars.get("feedback"), stored_cap, modern)
    provenance_path = _verify_descriptor(
        campaign, sidecars.get("structural_provenance"),
        contract["structural_provenance_max_bytes"])
    feedback = parse_feedback(feedback_path, stored_cap, decoded_cap, record_cap)
    if modern:
        descriptor = sidecars["feedback"]
        _verify_logical_descriptor(descriptor, feedback)
        _validate_modern_feedback(feedback)
    provenance = parse_provenance(
        provenance_path, contract["structural_provenance_max_bytes"])
    require(feedback == telemetry.get("feedback"), "feedback summary mismatch")
    require(provenance == telemetry.get("structural_provenance"),
            "provenance summary mismatch")
    require(feedback["complete"], "incomplete feedback lifecycle")
    require(provenance["complete"], "incomplete structural provenance")
    metadata = feedback["metadata"] or {}
    require(metadata.get("build-sha256") == arm["sha256"],
            "feedback build hash mismatch")
    require(metadata.get("input-sha256") == bindings.get("decompressed_input_sha256") ==
            provenance["input_file_sha256"], "decompressed input binding mismatch")
    require(metadata.get("policy") == "random-controller" and
            metadata.get("structural-provenance") == "1",
            "feedback policy/provenance contract mismatch")
    raw = raw_yield(feedback, provenance, result.get("solver_statistics"))
    require(raw == telemetry.get("raw_yield"), "raw GF2 yield mismatch")
    _cross_check(feedback, provenance, raw)
    checked = independent_checked(result)
    return {"task": result["task"], "instance": result["instance"],
            "classification": classify(raw, checked),
            "independent_checked_verdict": checked, "raw_yield": raw}


def score_campaign(campaign):
    campaign = Path(campaign).resolve()
    matrix = matrix_rows(campaign)
    expected = {f"{task:03d}.json" for task in matrix}
    present = {path.name for path in (campaign / "results").glob("*.json")}
    require(present == expected,
            f"result set mismatch: missing={sorted(expected-present)} "
            f"extra={sorted(present-expected)}")
    rows = [validate_result(campaign, campaign / "results" / f"{task:03d}.json")
            for task in matrix]
    return {"schema": "tactix-experimental-gf2-target-score-v1", "rows": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = score_campaign(args.campaign)
    except (OSError, KeyError, TypeError, ValueError) as error:
        print(f"GF2 target scoring failed: {error}", file=sys.stderr)
        return 1
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
