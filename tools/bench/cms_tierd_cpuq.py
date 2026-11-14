"""Checked-row predicates for the closed CryptoMiniSat Tier-D campaigns.

The CG collection builder reads published CMS rows through these predicates;
the campaign runners are in git history.
"""

import re
from pathlib import Path


def family(instance):
    name = Path(instance).name.split("-", 1)[-1]
    return re.split(r"[-_.]", name, maxsplit=1)[0].lower()


def independently_checked(result, manifest):
    if not result.get("valid"):
        return False
    if (result.get("expected_match") is not True or
            result.get("solver") != manifest["solvers"][0]["name"] or
            result.get("solve", {}).get("timed_out") or
            result.get("solve", {}).get("exit_code") != 0 or
            not result.get("certificate_bytes") or
            not result.get("certificate_sha256")):
        raise RuntimeError("published valid row lacks a checked solver certificate")
    if result["verdict"] == "SAT":
        check = result.get("model_check") or {}
        if (result.get("model_ok") is not True or
                result.get("model_protocol_ok") is not True or
                result.get("model_checker", {}).get("sha256") !=
                manifest["provenance"]["checkmodel"]["sha256"] or
                check.get("result") != "verified" or
                check.get("exit_code") != 0 or check.get("timed_out")):
            raise RuntimeError("published SAT row lacks independent model check")
    elif result["verdict"] == "UNSAT":
        check = result.get("proof_check") or {}
        stages = check.get("stages") or []
        storage = manifest.get("certificate_policy", {}).get("storage")
        temporary_key = ("temporary_deleted" if storage == "workspace-ephemeral"
                         else "anonymous_temp_deleted")
        if (result.get("proof_ok") is not True or
                check.get("result") != "verified" or
                check.get("pipeline") != "frat-xor-cake-xlrup" or
                len(stages) != 2 or
                [stage.get("name") for stage in stages] !=
                ["frat-xor", "cake-xlrup"] or
                any(stage.get("result") != "verified" or
                    stage.get("exit_code") != 0 or stage.get("timed_out") or
                    stage.get("input_artifact_deleted") is not True or
                    (stage["name"] == "frat-xor" and
                     stage.get(temporary_key) is not True) or
                    stage.get("provenance", {}).get("sha256") !=
                    manifest["provenance"][stage["name"]]["sha256"]
                    for stage in stages) or
                any(artifact.get("deleted") is not True
                    for artifact in check.get("derived_artifacts", []))):
            raise RuntimeError("published UNSAT row lacks independent proof check")
    else:
        raise RuntimeError("published valid row has no SAT/UNSAT verdict")
    return True


def verified_saved_arms(result, arms):
    """Compare only independently verified saved arms with a checked CMS row."""
    checked = [arm for arm in arms if arm["verified"] == "True"]
    if any(arm["expected"] != result["expected"] or
           arm["verdict"] != result["verdict"] for arm in checked):
        raise RuntimeError("saved verified verdict disagrees with CMS")
    return checked
