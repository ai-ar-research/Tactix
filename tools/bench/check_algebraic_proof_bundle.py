#!/usr/bin/env python3
"""Verify batched algebraic lemmas and their single DSR consumer proof.

This is a composition coordinator, not a new proof calculus.  It constructs
the exact selector challenges and augmented CNF, dispatches pinned backend
checkers, and rejects any source, lemma, proof, or checker mutation.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from check_xor_dsr_composition import (checked_run, parse_clause, read_cnf,
                                       scan_original, write_augmented, write_cnf,
                                       INT32_MAX)

SCHEMA = "tactix-algebraic-proof-bundle-v1"
HEX64 = re.compile(r"[0-9a-f]{64}")


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def exact_keys(value, keys, label):
    if not isinstance(value, dict) or set(value) != set(keys):
        raise ValueError(f"{label} fields differ from schema")


def resolve_ref(root, value, label):
    exact_keys(value, ("path", "sha256"), label)
    relative = Path(value["path"])
    digest = value["sha256"]
    if (relative.is_absolute() or not relative.parts or ".." in relative.parts or
            not isinstance(digest, str) or not HEX64.fullmatch(digest)):
        raise ValueError(f"invalid {label} reference")
    unresolved = root / relative
    if unresolved.is_symlink():
        raise ValueError(f"{label} must not be a symbolic link")
    path = unresolved.resolve()
    try:
        path.relative_to(root)
    except ValueError:
        raise ValueError(f"{label} escapes bundle directory")
    if not path.is_file() or path.is_symlink() or sha(path) != digest:
        raise ValueError(f"{label} file or digest mismatch")
    return path


def canonical_lemmas(path, variables):
    lemmas = []
    with path.open() as stream:
        for number, raw in enumerate(stream, 1):
            if not raw.endswith("\n") or not raw.strip() or raw.startswith("c"):
                raise ValueError(f"noncanonical lemma line {number}")
            clause = parse_clause(raw.strip(), variables)
            canonical = " ".join(map(str, clause)) + " 0\n"
            if raw != canonical:
                raise ValueError(f"noncanonical lemma bytes at line {number}")
            lemmas.append(clause)
    if (not lemmas or len(lemmas) > INT32_MAX - variables or
            len({tuple(sorted(x)) for x in lemmas}) != len(lemmas)):
        raise ValueError("lemma table is empty or contains duplicate clauses")
    return lemmas


def proof_oracle_command(args, cnf, proof):
    return [sys.executable, args.proof_oracle, "check-dsr", cnf, proof,
            "--dsr-trim", args.dsr_trim,
            "--dsr-compress", args.dsr_compress,
            "--srcheck", args.srcheck, "--every-lemma"]


def selector_challenge(source, selected, variables):
    """Return a CNF that is satisfiable iff some selected lemma is not implied.

    A true selector chooses one lemma and forces every literal in that lemma
    false.  The positive selector clause requires at least one choice.  Keeping
    this construction pure gives the exhaustive semantic control a direct hook
    into the exact production transformation.
    """
    selectors = list(range(variables + 1, variables + len(selected) + 1))
    challenge = list(source) + [selectors]
    challenge += [[-selector, -literal]
                  for selector, clause in zip(selectors, selected)
                  for literal in clause]
    return selectors, challenge


def verify_batch(batch, source, lemmas, variables, args):
    selected = [lemmas[index - 1] for index in batch["lemma_ids"]]
    selectors, challenge = selector_challenge(source, selected, variables)
    with tempfile.TemporaryDirectory(
            prefix=f"tactix-bundle-{batch['id']}-", dir=args.scratch) as temp:
        work = Path(temp)
        challenge_path = work / "challenge.cnf"
        write_cnf(challenge_path, variables + len(selected), challenge)
        env = {**os.environ, "TMPDIR": temp,
               "FRAT_XOR_TEMP_PATH": str(work / "frat-xor-elab.temp"),
               "CML_HEAP_SIZE": str(args.cake_heap_mib)}
        if batch["backend"] == "xor-xlrup-v1":
            elaborated_path = work / "challenge.xlrup"
            elaborated = subprocess.run(
                [str(args.frat_xor), "elab", str(batch["certificate_path"]),
                 str(challenge_path), str(elaborated_path)],
                capture_output=True, text=True, timeout=args.timeout, env=env)
            if elaborated.returncode:
                raise RuntimeError(
                    f"batch {batch['id']} FRAT-XOR elaboration failed: " +
                    elaborated.stderr[-1000:])
            output = checked_run(
                [args.cake_xlrup, challenge_path, elaborated_path],
                args.timeout, env, "s VERIFIED UNSAT")
            backend_result = {"cake_xlrup_verified": True,
                              "checker_stdout": output,
                              "elaborated_sha256": sha(elaborated_path)}
        elif batch["backend"] == "dsr-selector-v1":
            output = checked_run(
                proof_oracle_command(args, challenge_path,
                                     batch["certificate_path"]),
                args.timeout, env, "s VERIFIED UNSAT")
            backend_result = {"every_lemma_dsr_verified": True,
                              "checker_stdout": output}
        else:
            raise ValueError(f"unsupported algebraic backend: {batch['backend']}")
        return {"id": batch["id"], "backend": batch["backend"],
                "lemma_ids": batch["lemma_ids"],
                "source_clauses": len(source),
                "challenge_clauses": len(challenge),
                "challenge_sha256": sha(challenge_path), **backend_result}


def load_bundle(manifest_path):
    manifest_path = manifest_path.resolve()
    root = manifest_path.parent.resolve()
    manifest = json.loads(manifest_path.read_text())
    exact_keys(manifest, ("schema", "original", "lemmas", "batches", "consumer"),
               "bundle")
    if manifest["schema"] != SCHEMA:
        raise ValueError("unsupported proof-bundle schema")
    original = resolve_ref(root, manifest["original"], "original")
    lemma_path = resolve_ref(root, manifest["lemmas"], "lemmas")
    consumer = manifest["consumer"]
    exact_keys(consumer, ("backend", "certificate"), "consumer")
    if consumer["backend"] != "dsr-v1":
        raise ValueError("bundle consumer must be native DSR")
    consumer_path = resolve_ref(root, consumer["certificate"], "consumer certificate")
    if not isinstance(manifest["batches"], list) or not manifest["batches"]:
        raise ValueError("bundle must contain at least one algebraic batch")
    batches = []
    names = set()
    for ordinal, batch in enumerate(manifest["batches"]):
        exact_keys(batch, ("id", "backend", "source", "certificate", "lemma_ids"),
                   f"batch {ordinal}")
        name = batch["id"]
        ids = batch["lemma_ids"]
        if (not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or
                name in names or not isinstance(ids, list) or not ids or
                any(isinstance(x, bool) or not isinstance(x, int) or x <= 0 for x in ids) or
                len(set(ids)) != len(ids) or ids != sorted(ids)):
            raise ValueError(f"invalid batch identity or lemma IDs: {ordinal}")
        names.add(name)
        batches.append({**batch,
                        "source_path": resolve_ref(root, batch["source"],
                                                   f"batch {name} source"),
                        "certificate_path": resolve_ref(
                            root, batch["certificate"],
                            f"batch {name} certificate")})
    return manifest, root, original, lemma_path, consumer_path, batches


def verify(args):
    (manifest, root, original, lemma_path,
     consumer_path, batches) = load_bundle(args.bundle)
    stable = {"bundle": args.bundle.resolve(), "original": original,
              "lemmas": lemma_path, "consumer": consumer_path,
              "frat_xor": args.frat_xor.resolve(),
              "cake_xlrup": args.cake_xlrup.resolve(),
              "proof_oracle": args.proof_oracle.resolve(),
              "dsr_trim": args.dsr_trim.resolve(),
              "dsr_compress": args.dsr_compress.resolve(),
              "srcheck": args.srcheck.resolve()}
    coordinator = Path(__file__).resolve()
    stable["coordinator"] = coordinator
    for batch in batches:
        stable[f"source:{batch['id']}"] = batch["source_path"]
        stable[f"certificate:{batch['id']}"] = batch["certificate_path"]
    pinned = {name: sha(path) for name, path in stable.items()}

    # Parse each declared source before the one original-CNF scan.  Reuse of
    # one original occurrence by several batches is legal, hence maximum
    # requested multiplicity rather than a sum across batches.
    source_clauses = {}
    required = Counter()
    source_variables = {}
    for batch in batches:
        variables, clauses = read_cnf(batch["source_path"])
        source_variables[batch["id"]] = variables
        source_clauses[batch["id"]] = clauses
        counts = Counter(tuple(sorted(clause)) for clause in clauses)
        for clause, count in counts.items():
            required[clause] = max(required[clause], count)
    remaining = required.copy()
    variables, original_count = scan_original(original, remaining)
    if any(remaining.values()):
        raise ValueError("an algebraic source is not a clause-multiset subset of original")
    if any(value > variables for value in source_variables.values()):
        raise ValueError("algebraic source declares variables outside original CNF")
    lemmas = canonical_lemmas(lemma_path, variables)
    ownership = [index for batch in batches for index in batch["lemma_ids"]]
    if sorted(ownership) != list(range(1, len(lemmas) + 1)):
        raise ValueError("algebraic batches must cover every lemma exactly once")

    workers = min(args.jobs, len(batches))
    checked = []
    with ThreadPoolExecutor(max_workers=workers) as pool:
        futures = {pool.submit(verify_batch, batch,
                               source_clauses[batch["id"]], lemmas,
                               variables, args): batch["id"]
                   for batch in batches}
        for future in as_completed(futures):
            checked.append(future.result())
    checked.sort(key=lambda item: item["id"])

    with tempfile.TemporaryDirectory(prefix="tactix-bundle-consumer-",
                                     dir=args.scratch) as temp:
        augmented = Path(temp) / "augmented.cnf"
        write_augmented(augmented, original, variables, original_count, lemmas)
        env = {**os.environ, "TMPDIR": temp,
               "CML_HEAP_SIZE": str(args.cake_heap_mib)}
        consumer_output = checked_run(
            proof_oracle_command(args, augmented, consumer_path),
            args.timeout, env, "s VERIFIED UNSAT")
        augmented_sha = sha(augmented)

    if any(sha(path) != pinned[name] for name, path in stable.items()):
        raise RuntimeError("bundle input or checker changed during verification")
    return {"schema": "tactix-algebraic-proof-bundle-result-v1",
            "bundle_schema": manifest["schema"],
            "bundle_root": str(root),
            "original_clauses": original_count,
            "lemmas": len(lemmas), "batches": checked,
            "source_subset_checked": True,
            "lemma_ownership_checked": True,
            "consumer_every_lemma_dsr_verified": True,
            "consumer_stdout": consumer_output,
            "augmented_sha256": augmented_sha,
            "sha256": pinned,
            "raw_intermediates_deleted": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    for name in ("frat-xor", "cake-xlrup", "proof-oracle", "dsr-trim",
                 "dsr-compress", "srcheck"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--scratch", type=Path, default=Path("/tmp"))
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--cake-heap-mib", type=int, default=4096)
    parser.add_argument("--jobs", type=int, default=1)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    if (args.timeout <= 0 or args.cake_heap_mib < 1024 or args.jobs <= 0 or
            not args.scratch.is_dir()):
        parser.error("invalid resource bound or scratch directory")
    result = verify(args)
    if args.report:
        with args.report.open("x") as output:
            json.dump(result, output, indent=2, sort_keys=True)
            output.write("\n")
    print("s VERIFIED TACTIX ALGEBRAIC PROOF BUNDLE")


if __name__ == "__main__":
    main()
