#!/usr/bin/env python3
"""Extract fail-closed, single-result bookkeeping for GF(2) target probes.

The report proves what the controller offered, what the solver committed, and
which later actions observed a change.  It deliberately does not turn those
facts into a counterfactual performance claim.
"""

import argparse
from collections import Counter
import gzip
import hashlib
import json
from pathlib import Path
import sys

from gf2_target_audit import audit_result
from gf2_target_score import AFFINE_COUNTERS, object_pairs, reject_constant
from inspect_policy_feedback import observation, observation_delta


SCHEMA = "tactix-experimental-gf2-bookkeeping-v1"
UINT32_MAX = (1 << 32) - 1
UINT64_MAX = (1 << 64) - 1
ROW_KINDS = (
    "direct-contradiction", "direct-unit", "direct-binary",
    "direct-small", "extended",
)
PROOF_KINDS = {
    "affine-basis-definition-rat", "affine-basis-helper-fix-rat",
    "affine-basis-proof-rat", "affine-basis-proof-rup",
    "affine-basis-proof-delete",
}
ADVICE_COUNTERS = (
    "semantic_variable_candidates_published",
    "semantic_variable_candidate_existing",
    "semantic_variable_candidate_introduced",
    "semantic_variable_advice_attempts",
    "semantic_variable_advice_applied",
    "semantic_variable_advice_invalid",
    "semantic_variable_advice_stale",
    "semantic_variable_advice_resource_exhausted",
    "semantic_variable_advice_inapplicable",
    "semantic_variable_advice_variables",
    "semantic_variable_advice_priority",
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def strict_json(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=object_pairs,
                      parse_constant=reject_constant)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def descriptor_path(campaign, descriptor):
    require(type(descriptor) is dict and type(descriptor.get("path")) is str,
            "missing sidecar descriptor")
    path = (campaign / descriptor["path"]).resolve()
    require(path.is_relative_to(campaign) and path.is_file(),
            "sidecar path is unavailable")
    return path


def _integer_token(token, *, signed=False):
    require(token and token == token.strip(), "empty/malformed integer token")
    if signed:
        require(token.removeprefix("-").isdigit(), "invalid signed integer")
    else:
        require(token.isdigit(), "invalid unsigned integer")
    value = int(token)
    if signed:
        require(-(1 << 31) <= value < (1 << 31), "signed integer overflow")
    else:
        require(0 <= value <= UINT64_MAX, "unsigned integer overflow")
    return value


class _Fields:
    def __init__(self, text):
        require(text, "empty affine row payload")
        self.values = text.split(",")
        require(all(self.values), "empty affine row field")
        self.at = 0

    def unsigned(self):
        require(self.at < len(self.values), "truncated affine row payload")
        value = _integer_token(self.values[self.at])
        self.at += 1
        return value

    def signed(self):
        require(self.at < len(self.values), "truncated affine row payload")
        value = _integer_token(self.values[self.at], signed=True)
        self.at += 1
        return value

    def done(self):
        require(self.at == len(self.values), "trailing affine row fields")

    def remaining(self):
        return len(self.values) - self.at


def _parse_affine_row(text):
    fields = _Fields(text)
    owner = fields.unsigned()
    elimination_profile = fields.unsigned()
    kind_code = fields.unsigned()
    require(owner and elimination_profile and kind_code < len(ROW_KINDS),
            "invalid affine row identity")
    width = fields.unsigned()
    require(width <= UINT32_MAX and width <= fields.remaining(),
            "affine row width overflow")
    variables = [fields.signed() for _ in range(width)]
    require(all(variable > 0 for variable in variables) and
            variables == sorted(set(variables)), "invalid affine row variables")
    rhs = fields.unsigned()
    require(rhs <= 1, "invalid affine row rhs")
    source_count = fields.unsigned()
    minimum = 1 if kind_code == 4 else 2
    require(minimum <= source_count <= fields.remaining(),
            "invalid affine source equation count")
    ordinals = [fields.unsigned() for _ in range(source_count)]
    require(ordinals == sorted(set(ordinals)), "invalid source ordinal order")
    equation_count = fields.unsigned()
    require(equation_count == source_count, "source equation count mismatch")
    equations = []
    for _ in range(equation_count):
        arity = fields.unsigned()
        require(0 < arity < 64 and arity <= fields.remaining(),
                "invalid source equation arity")
        equation_variables = [fields.signed() for _ in range(arity)]
        require(all(variable > 0 for variable in equation_variables) and
                equation_variables == sorted(set(equation_variables)),
                "invalid source equation variables")
        equation_rhs = fields.unsigned()
        require(equation_rhs <= 1, "invalid source equation rhs")
        premise_count = fields.unsigned()
        require(premise_count == 1 << (arity - 1),
                "source equation premise count mismatch")
        require(premise_count <= fields.remaining() // (arity + 2),
                "truncated source equation premises")
        premises = []
        for _ in range(premise_count):
            clause_id = fields.unsigned()
            clause_width = fields.unsigned()
            require(clause_id and clause_width == arity,
                    "invalid source premise identity")
            clause = [fields.signed() for _ in range(clause_width)]
            require(all(clause), "zero source literal")
            premises.append({"clause_id": clause_id, "clause": clause})
        equations.append({"variables": equation_variables,
                          "rhs": bool(equation_rhs), "premises": premises})
    fields.done()
    return {
        "owner_version": owner,
        "elimination_profile": elimination_profile,
        "kind_code": kind_code,
        "kind": ROW_KINDS[kind_code],
        "width": width,
        "variables": variables,
        "rhs": bool(rhs),
        "source_equation_ordinals": ordinals,
        "source_equations": equations,
    }


def parse_affine_invocation(invocation):
    """Decode the versioned public affine row spelling emitted by C++.

    No Python binding exists for ``deserialize_online_strategy_program``.
    This decoder implements only the bounded ``affine-basis-row1`` payload and
    rejects malformed/trailing fields rather than interpreting other grammar.
    """
    require(type(invocation) is str, "affine invocation is not text")
    marker = ":affine-basis-row1["
    start = invocation.find(marker)
    require(start >= 0 and invocation.find(marker, start + 1) < 0,
            "missing/repeated affine-basis-row1 payload")
    payload_start = start + len(marker)
    end = invocation.find("]", payload_start)
    require(end >= 0 and invocation.find("]", end + 1) < 0,
            "unterminated/repeated affine row terminator")
    require(payload_start < end, "empty affine row collection")
    rows = [_parse_affine_row(text)
            for text in invocation[payload_start:end].split(";")]
    require(rows, "empty affine row collection")
    return rows


def parse_advice_parameters(invocation):
    if type(invocation) is not str:
        return None
    marker = "semantic-variable-advice1("
    start = invocation.find(marker)
    if start < 0 or invocation.find(marker, start + 1) >= 0:
        return None
    start += len(marker)
    end = invocation.find(")", start)
    if end < 0:
        return None
    try:
        values = [_integer_token(item) for item in
                  invocation[start:end].split(",")]
    except ValueError:
        return None
    if len(values) != 4:
        return None
    return dict(zip(("maximum_variables", "maximum_priority_sum",
                     "existing_theory_priority",
                     "introduced_theory_priority"), values))


def feedback_actions(path, decide_variables=None):
    """Stream and reconstruct v1/v2 action observations.

    Only bookkeeping actions are retained.  Decide is additionally filtered
    to variables named by committed provenance, keeping memory bounded even
    when a target performs millions of ordinary decisions.
    """
    actions = []
    with Path(path).open("rb") as raw:
        compressed = raw.read(2) == b"\x1f\x8b"
    source = gzip.open(path, "rt") if compressed else Path(path).open()
    version = None
    current = None
    ended = False
    sequence = 0
    try:
        for number, line in enumerate(source, 1):
            require(line.endswith("\n"), "feedback has an unterminated record")
            row = json.loads(line, object_pairs_hook=object_pairs,
                             parse_constant=reject_constant)
            kind = row.get("type")
            if version is None:
                require(kind == "header" and row.get("version") in (1, 2),
                        "unsupported feedback header")
                version = row["version"]
            elif current is None:
                require(kind == "session_begin", "missing feedback session begin")
                observation(row["observation"])
                current = row["observation"]
            elif kind == "elapsed_nanos":
                continue
            elif kind == "session_end":
                require(not ended, "repeated feedback session end")
                final = (row["observation"] if version == 1 else
                         observation_delta(current, row["observation_delta"]))
                observation(final, validate_native=False)
                current, ended = final, True
            elif kind in ("action", "interval"):
                require(not ended and row.get("version") == version,
                        "record outside feedback lifecycle")
                if version == 1:
                    before, after = row["before"], row["after"]
                    observation(before)
                    observation(after)
                else:
                    before = observation_delta(current, row["before_delta"])
                    after = observation_delta(before, row["after_delta"])
                current = after
                if kind == "action":
                    sequence += 1
                    require(row.get("sequence") == sequence,
                            "feedback action sequence gap")
                    identity = row.get("identity")
                    retain = identity in {
                        "AffineConsequence", "Propagate",
                        "SemanticVariableAdvice",
                    }
                    if identity == "Decide":
                        literal = _signed_dimacs(row.get("instance"))
                        retain = (literal is not None and
                                  (decide_variables is None or
                                   abs(literal) in decide_variables))
                    if retain:
                        copied = dict(row)
                        copied["before"], copied["after"] = before, after
                        actions.append(copied)
            else:
                raise ValueError(f"line {number}: unexpected feedback record")
    finally:
        source.close()
    require(ended, "incomplete feedback lifecycle")
    return version, actions


def _native_delta(action, names, *, monotone=True):
    before, after = action["before"]["native"], action["after"]["native"]
    result = {}
    for name in names:
        require(name in before and name in after, f"missing native field {name}")
        delta = after[name] - before[name]
        require(not monotone or delta >= 0, f"non-monotone native field {name}")
        result[name] = delta
    return result


def provenance_transactions(path):
    records = []
    with Path(path).open() as source:
        for number, line in enumerate(source, 1):
            require(line.endswith("\n"), "provenance has an unterminated record")
            row = json.loads(line, object_pairs_hook=object_pairs,
                             parse_constant=reject_constant)
            require(type(row) is dict, f"provenance line {number} is not an object")
            records.append(row)
    require(records and records[0].get("kind") == "session",
            "missing provenance session")
    transactions, active = [], None
    for row in records[1:]:
        identity = (row.get("action"), row.get("semantic"), row.get("owner"))
        if row.get("kind") == "affine-basis-begin":
            require(active is None, "nested provenance transaction")
            active = {"identity": identity, "records": [row]}
            continue
        require(active is not None and identity == active["identity"],
                "provenance record outside transaction")
        active["records"].append(row)
        if row.get("kind") == "affine-basis-complete":
            transactions.append(_summarize_transaction(active))
            active = None
    require(active is None, "incomplete provenance transaction")
    return records[0], transactions


def _summarize_transaction(transaction):
    action, semantic, owner = transaction["identity"]
    records = transaction["records"]
    begin = records[0]
    sources = [row for row in records if row["kind"] == "affine-basis-source"]
    rows = [row for row in records if row["kind"] == "affine-basis-row"]
    proof = [row for row in records if row["kind"] in PROOF_KINDS]
    definitions = [row for row in records
                   if row["kind"] == "affine-basis-theory-variable"]
    helpers = [row for row in records
               if row["kind"] == "affine-basis-proof-helper"]
    complete = records[-1]
    require(complete["kind"] == "affine-basis-complete",
            "transaction lacks complete record")
    event_counts = Counter(row["kind"] for row in proof)
    installed = [row for row in proof
                 if row["kind"] != "affine-basis-proof-delete" and row["install"]]
    temporary = [row for row in proof
                 if row["kind"] != "affine-basis-proof-delete" and not row["install"]]
    deleted = [row for row in proof if row["kind"] == "affine-basis-proof-delete"]
    require(len(installed) == complete["published"],
            "installed/provenance publication mismatch")
    require(len(proof) - len(deleted) == complete["additions"] and
            len(deleted) == complete["deletions"],
            "proof event/provenance total mismatch")
    return {
        "action_id": action, "semantic_id": semantic, "owner_version": owner,
        "begin": {name: begin[name] for name in
                  ("equations", "rows", "definitions", "propagation")},
        "source_clauses": [
            {name: row[name] for name in ("occurrence", "source_id", "clause")}
            for row in sources],
        "rows": [
            {**{name: row[name] for name in
                ("row", "rhs", "variables", "sources", "premises")},
             "width": len(row["variables"])} for row in rows],
        "proof_events_by_kind": dict(sorted(event_counts.items())),
        "additions": complete["additions"],
        "deletions": complete["deletions"],
        "published": complete["published"],
        "installed_clauses": installed,
        "temporary_clauses": temporary,
        "deleted_clauses": deleted,
        "definitions": definitions,
        "proof_helpers": helpers,
        "extent_growth": complete["extent_growth"],
    }


def _selection_premises(selection):
    return sorted((premise["clause_id"], tuple(premise["clause"]))
                  for equation in selection["source_equations"]
                  for premise in equation["premises"])


def _committed_premises(committed):
    return sorted((source["source_id"], tuple(source["clause"]))
                  for source in committed["source_clauses"])


def join_transactions(actions, transactions):
    selected = []
    for action in actions:
        if action.get("identity") != "AffineConsequence":
            continue
        delta = _native_delta(action, AFFINE_COUNTERS)
        invocation = (action.get("request") or {}).get("invocation")
        rows = None
        if type(invocation) is str and ":affine-basis-row1[" in invocation:
            rows = parse_affine_invocation(invocation)
        if delta["affine_consequence_applied"]:
            require(delta["affine_consequence_applied"] == 1 and rows,
                    "applied affine action lacks one exact selection")
            selected.append((action, delta, rows))
    require(len(selected) == len(transactions),
            "feedback/provenance transaction count mismatch")
    joined = []
    for (action, delta, selections), committed in zip(selected, transactions):
        require(len(selections) == 1, "applied request contains multiple affine rows")
        selection = selections[0]
        require(selection["owner_version"] == committed["owner_version"],
                "feedback/provenance owner mismatch")
        require(len(committed["rows"]) == 1, "committed transaction has multiple rows")
        row = committed["rows"][0]
        require((selection["variables"], selection["rhs"],
                 selection["source_equation_ordinals"]) ==
                (row["variables"], row["rhs"], row["sources"]),
                "feedback/provenance row mismatch")
        require(_selection_premises(selection) == _committed_premises(committed),
                "feedback/provenance premise mismatch")
        work_names = (
            "affine_consequence_snapshot_clauses",
            "affine_consequence_snapshot_literals",
            "affine_consequence_snapshot_work",
            "affine_consequence_recognition_work",
            "affine_consequence_basis_work",
            "affine_consequence_proof_work",
            "affine_consequence_journal_work",
            "affine_consequence_journal_bytes",
            "affine_consequence_session_work",
            "affine_consequence_preflight_additions",
            "affine_consequence_preflight_deletions",
            "affine_consequence_additions",
            "affine_consequence_deletions",
            "affine_consequence_transaction_keys",
            "affine_consequence_committed_keys",
            "affine_consequence_excluded_rows",
        )
        joined.append({
            "feedback_sequence": action["sequence"],
            "action_outcome": action["outcome"],
            "row_selection": selection,
            "committed": committed,
            "work": {name: delta[name] for name in work_names},
        })
    return joined


def _signed_dimacs(instance):
    if type(instance) is not int or not 0 < instance <= UINT32_MAX:
        return None
    return instance - (1 << 32) if instance >= (1 << 31) else instance


def followups(actions, transactions):
    transaction_by_sequence = {row["feedback_sequence"]: row for row in transactions}
    committed_by_id = {row["committed"]["action_id"]: row for row in transactions}
    propagations, advice, decisions = [], [], []
    latest = None
    for action in actions:
        if action["sequence"] in transaction_by_sequence:
            latest = transaction_by_sequence[action["sequence"]]
        identity = action.get("identity")
        if identity == "Propagate":
            native = _native_delta(action, ("root_assignments", "propagations", "ticks"))
            propagations.append({
                "sequence": action["sequence"],
                "preceding_affine_action_id":
                    (latest["committed"]["action_id"] if latest else None),
                "attribution": "first-propagate-after-affine" if latest else "unattributed",
                "outcome": action["outcome"],
                "before_fixpoint": action["before"]["fixpoint"],
                "after_fixpoint": action["after"]["fixpoint"],
                "delta_root_population": (action["after"]["root_population"] -
                                          action["before"]["root_population"]),
                "delta_root_assignments": native["root_assignments"],
                "delta_propagations": native["propagations"],
                "delta_ticks": native["ticks"],
                "inconsistent": action["after"]["inconsistent"],
                "conflict": action["after"]["conflict"],
            })
            latest = None
        elif identity == "SemanticVariableAdvice":
            candidates = action["before"].get("semantic_variable_candidates", [])
            candidate = next((item for item in candidates
                              if item["id"] == action.get("instance")), None)
            delta = _native_delta(action, ADVICE_COUNTERS)
            source_id = candidate.get("action_id") if candidate else None
            advice.append({
                "sequence": action["sequence"],
                "candidate_id": action.get("instance"),
                "source_action_id": source_id,
                "source_is_committed_affine": source_id in committed_by_id,
                "existing_count": (candidate.get("existing_variables")
                                   if candidate else None),
                "introduced_count": (candidate.get("introduced_variables")
                                     if candidate else None),
                "configured_parameters": parse_advice_parameters(
                    (action.get("request") or {}).get("invocation")),
                "outcome": action["outcome"],
                "native_delta": delta,
            })
        elif identity == "Decide":
            literal = _signed_dimacs(action.get("instance"))
            if literal is not None:
                decisions.append({"sequence": action["sequence"],
                                  "dimacs_literal": literal})
    return {"propagate": propagations, "advice": advice}, decisions


def _integrity(telemetry):
    feedback = telemetry["feedback"]
    provenance = telemetry["structural_provenance"]
    descriptors = telemetry["sidecars"]
    return {
        "feedback": {
            "complete": feedback["complete"],
            "wire_version": feedback.get("wire_version", 1),
            "stored_bytes": feedback.get("stored_bytes", feedback.get("bytes")),
            "logical_bytes": feedback["bytes"],
            "records": feedback["lines"],
            "logical_sha256": feedback.get("logical_sha256"),
            "descriptor": descriptors["feedback"],
        },
        "provenance": {
            "complete": provenance["complete"],
            "bytes": provenance["bytes"],
            "records": provenance["lines"],
            "descriptor": descriptors["structural_provenance"],
        },
    }


def build_report(campaign, result_path):
    campaign = Path(campaign).resolve()
    result_path = Path(result_path).resolve()
    audit = audit_result(campaign, result_path)
    require(audit.get("evidence_complete"),
            "GF2 audit failed: " + json.dumps(audit.get("issues", []), sort_keys=True))
    result = strict_json(result_path)
    manifest = strict_json(campaign / "manifest.json")
    telemetry = result["gf2_telemetry"]
    feedback_path = descriptor_path(campaign, telemetry["sidecars"]["feedback"])
    provenance_path = descriptor_path(
        campaign, telemetry["sidecars"]["structural_provenance"])
    _, committed = provenance_transactions(provenance_path)
    decide_variables = {
        abs(variable)
        for transaction in committed
        for row in transaction["rows"]
        for variable in row["variables"]
    }
    decide_variables.update(
        item["output"] for transaction in committed
        for item in transaction["definitions"])
    wire_version, actions = feedback_actions(feedback_path, decide_variables)
    transactions = join_transactions(actions, committed)
    action_followups, decisions = followups(actions, transactions)

    row_variables = {}
    introduced = {}
    for transaction in transactions:
        sequence = transaction["feedback_sequence"]
        for variable in transaction["row_selection"]["variables"]:
            row_variables[variable] = min(sequence, row_variables.get(variable, sequence))
        for item in transaction["committed"]["definitions"]:
            variable = item["output"]
            introduced[variable] = min(sequence, introduced.get(variable, sequence))
    branch_rows = [event for event in decisions
                   if abs(event["dimacs_literal"]) in row_variables and
                   event["sequence"] > row_variables[abs(event["dimacs_literal"])]]
    branch_introduced = [event for event in decisions
                         if abs(event["dimacs_literal"]) in introduced and
                         event["sequence"] > introduced[abs(event["dimacs_literal"])]]
    effective_sequences = {row["feedback_sequence"] for row in transactions}
    immediate = any(item["preceding_affine_action_id"] is not None and
                    (item["delta_root_population"] > 0 or item["conflict"] or
                     item["inconsistent"])
                    for item in action_followups["propagate"])
    advised = any(item["source_is_committed_affine"] and
                  item["native_delta"]["semantic_variable_advice_applied"] > 0
                  for item in action_followups["advice"])
    checked = audit["frozen_score_row"]["independent_checked_verdict"]
    profile_hash = next((value for name, value in manifest["pin_sha256"].items()
                         if name.endswith("/gf2-affine.profile") or
                         name == "pins/gf2-affine.profile"), None)
    bindings = telemetry["bindings"]
    report = {
        "schema": SCHEMA,
        "identity": {
            "task": result["task"], "rank": result["rank"],
            "instance": result["instance"], "expected": result["expected"],
            "manifest_sha256": bindings["manifest_sha256"],
            "binary_sha256": bindings["binary_sha256"],
            "compressed_input_sha256": bindings["compressed_input_sha256"],
            "decompressed_input_sha256": bindings["decompressed_input_sha256"],
            "profile_sha256": profile_hash,
        },
        "integrity": _integrity(telemetry),
        "solve": {
            "verdict": result["verdict"], "solve": result.get("solve"),
            "certificate_bytes": result.get("certificate_bytes"),
            "certificate_sha256": result.get("certificate_sha256"),
            "proof_check": result.get("proof_check"),
            "model_check": result.get("model_check"),
            "valid": result.get("valid"),
        },
        "producer": telemetry["raw_yield"]["producer"],
        "transactions": transactions,
        "followups": action_followups,
        "branch_observation": {
            "decide_events_on_committed_row_variables": branch_rows,
            "decide_events_on_introduced_variables": branch_introduced,
            "attribution": "observed-only",
        },
        "assessment": {
            "exact_nontrivial_offer": any(
                len(row["row_selection"]["source_equations"]) >= 2
                for row in transactions),
            "committed_certified_clauses": any(
                row["committed"]["published"] > 0 for row in transactions),
            "immediate_root_effect": immediate,
            "advice_applied": advised,
            "theory_variable_later_decided": bool(branch_rows or branch_introduced),
            "independently_checked_terminal_result": checked,
            "causal_performance_claim": False,
        },
        "validation": {
            "audit_schema": audit["schema"],
            "frozen_score_row": audit["frozen_score_row"],
            "feedback_wire_version": wire_version,
            "effective_transaction_sequences": sorted(effective_sequences),
        },
        "limitations": [
            "semantic candidate observations retain role counts, not exact advised variable identities",
            "a Decide event on a theory variable does not prove that advice caused the choice",
            "installed clauses are proven live at transaction commit, not at solver termination",
            "a first Propagate action after affine publication is sequence-window evidence, not an explicit causal edge",
            "certificate bytes cover the whole solve; no per-transaction proof byte extent is retained",
            "CertifiedBasisV2 charges wide derivation work to basis_work; proof_work can remain zero",
            "one run cannot establish a counterfactual runtime benefit",
        ],
    }
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path)
    parser.add_argument("--result", required=True, type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = build_report(args.campaign, args.result)
    except (OSError, KeyError, TypeError, ValueError) as error:
        print(f"GF2 bookkeeping failed: {error}", file=sys.stderr)
        return 1
    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
