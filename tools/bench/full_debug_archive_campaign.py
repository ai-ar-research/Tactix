#!/usr/bin/env python3
"""Prepare, freeze, and verify an archived current-worktree full-Debug run."""

from __future__ import annotations

import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import shutil
import stat
import subprocess
import tarfile
from typing import Optional

from full_debug_archive import ArchiveHarnessError, inside, prepare_clean_build, validate_test_commands


ROOT = Path(__file__).resolve().parents[2]
RUNS = ROOT / "benchmarks/runs"
BUILD_ROOT = ROOT / "build"
MUTABLE = (Path("build"), Path("tools/fuzz/failures"))
CHECKERS = ("cake_lpr", "drat-trim", "dsr-compress", "dsr-trim", "lrat-check", "srcheck")
SCHEMA = "worktree-archived-source-full-debug-v3"
TEMPLATE_SCHEMA = "worktree-archived-source-full-debug-template-v3"


def output(*argv: str) -> bytes:
    return subprocess.run(argv, cwd=ROOT, check=True, stdout=subprocess.PIPE).stdout


def sha_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha_file(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def relative(path: Path) -> str:
    return path.resolve(strict=True).relative_to(ROOT.resolve(strict=True)).as_posix()


def copy_entry(source: Path, target: Path, *, dereference: bool = False) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.is_symlink() and not dereference:
        target.symlink_to(os.readlink(source))
    else:
        shutil.copy2(source.resolve() if dereference else source, target)


def source_entries(run: Path) -> list[Path]:
    raw = output("git", "ls-files", "-z", "--cached", "--others", "--exclude-standard")
    excluded = run.relative_to(ROOT)
    entries = []
    for name in raw.split(b"\0"):
        if not name:
            continue
        path = Path(os.fsdecode(name))
        if path == excluded or excluded in path.parents:
            continue
        candidate = ROOT / path
        if not candidate.exists() and not candidate.is_symlink():
            raise ArchiveHarnessError(f"listed worktree input is missing: {path}")
        entries.append(path)
    if not entries:
        raise ArchiveHarnessError("worktree source list is empty")
    return sorted(entries, key=Path.as_posix)


def entry_digest(entries: list[Path]) -> str:
    value = hashlib.sha256()
    for relative_path in entries:
        path = ROOT / relative_path
        mode = path.lstat().st_mode
        if stat.S_ISLNK(mode):
            kind, payload = b"l", os.readlink(path).encode()
        elif stat.S_ISREG(mode):
            kind, payload = b"f", bytes.fromhex(sha_file(path))
        else:
            raise ArchiveHarnessError(f"unsupported worktree input: {relative_path}")
        value.update(kind + b"\0" + relative_path.as_posix().encode() + b"\0" +
                     (b"x" if mode & 0o111 else b"-") + b"\0" + payload + b"\0")
    return value.hexdigest()


def tree_digest(root: Path, mutable: list[str]) -> str:
    value = hashlib.sha256()
    for path in sorted(root.rglob("*"), key=lambda p: p.relative_to(root).as_posix()):
        rel = path.relative_to(root).as_posix()
        if any(rel.startswith(prefix + "/") for prefix in mutable):
            continue
        mode = path.lstat().st_mode
        kind = b"d" if stat.S_ISDIR(mode) else b"l" if stat.S_ISLNK(mode) else b"f"
        payload = os.readlink(path).encode() if kind == b"l" else bytes.fromhex(sha_file(path)) if kind == b"f" else b""
        value.update(kind + b"\0" + rel.encode() + b"\0" +
                     (b"x" if mode & 0o111 else b"-") + b"\0" + payload + b"\0")
    return value.hexdigest()


def copy_dependencies(source: Path) -> list[str]:
    dependencies = [
        "benchmarks/reports/2026-09-14-orbitopal-certified-claude-fuzz/terminal-report.md",
        "benchmarks/baselines/sc24-26-32g-5000s/manifest.json",
        "benchmarks/baselines/sc24-26-32g-5000s/pins/veripb",
        "benchmarks/baselines/sc24-26-32g-5000s/pins/cake-pb-cnf",
        "benchmarks/baselines/sc24-26-32g-5000s/pins/dsr-trim",
        "benchmarks/baselines/sc24-26-32g-5000s/pins/dsr-compress",
        "benchmarks/baselines/sc24-26-32g-5000s/pins/srcheck",
        "benchmarks/runs/20260921-am333-lymphosat-models-wv100/pins/dsr-trim",
        "benchmarks/runs/20260921-am333-lymphosat-models-wv100/pins/dsr-compress",
        "benchmarks/runs/20260921-am333-lymphosat-models-wv100/pins/srcheck",
        "benchmarks/runs/20260923-am337-proof-lifetime-v2/pins/veripb",
        "benchmarks/runs/20260923-am337-proof-lifetime-v2/pins/cake-pb-cnf",
    ]
    for name in dependencies:
        copy_entry(ROOT / name, source / name)
    for name in CHECKERS:
        relative_path = Path("tools/checkers/bin") / name
        copy_entry(ROOT / relative_path, source / relative_path, dereference=True)
        (source / relative_path).chmod(0o550)
        dependencies.append(relative_path.as_posix())
    selected = set()
    tables = ROOT / "benchmarks/probes/public-theory-targets-v1"
    for table in sorted(tables.glob("*.tsv")):
        with table.open(newline="") as stream:
            rows = csv.DictReader((line for line in stream if line.strip() and not line.startswith("#")), delimiter="\t")
            selected.update(row["instance"] for row in rows)
    if len(selected) != 25:
        raise ArchiveHarnessError(f"expected 25 public target cache files, found {len(selected)}")
    for instance in sorted(selected):
        relative_path = Path("benchmarks/cache") / instance
        copy_entry(ROOT / relative_path, source / relative_path)
        dependencies.append(relative_path.as_posix())
    return sorted(dependencies)


def packaged_dependency_paths(source: Path, names: list[str]) -> set[Path]:
    dependencies = set()
    for name in names:
        pure = PurePosixPath(name)
        candidate = source.joinpath(*pure.parts)
        if (pure.is_absolute() or ".." in pure.parts or
                not candidate.is_file() or not inside(candidate, source)):
            raise ArchiveHarnessError(f"invalid packaged dependency: {name}")
        dependencies.add(candidate)
    if len(dependencies) != len(names):
        raise ArchiveHarnessError("duplicate packaged dependency")
    return dependencies


def canonical_archive(source: Path, archive_path: Path) -> None:
    mutable = {path.as_posix() for path in MUTABLE}
    with tarfile.open(archive_path, "w", format=tarfile.PAX_FORMAT) as archive:
        for path in sorted(source.rglob("*"), key=lambda p: p.relative_to(source).as_posix()):
            name = path.relative_to(source).as_posix()
            if any(name.startswith(root + "/") for root in mutable):
                continue
            info = archive.gettarinfo(str(path), arcname=name)
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            info.mtime = 0
            if info.isfile():
                with path.open("rb") as stream:
                    archive.addfile(info, stream)
            else:
                archive.addfile(info)


def run_logged(path: Path, argv: list[str]) -> None:
    with path.open("w") as log:
        subprocess.run(argv, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)


def inventory(build: Path) -> dict:
    data = subprocess.run(["ctest", "--test-dir", str(build), "--show-only=json-v1"],
                          cwd=ROOT, check=True, stdout=subprocess.PIPE).stdout
    return json.loads(data)


def canonical(value: dict) -> str:
    return sha_bytes(json.dumps(value, sort_keys=True, separators=(",", ":")).encode())


def write_batch(run: Path, build: Path) -> None:
    command = ROOT / "tools/bench/full_debug_archive_campaign.py"
    body = f'''#!/usr/bin/env bash
#SBATCH --job-name=current-debug-archive3
#SBATCH --partition=cpu-dense-hwulab-q
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=8
#SBATCH --mem=32768M
#SBATCH --time=08:00:00
#SBATCH --no-requeue
#SBATCH --output={run}/slurm-%j.log

set -euo pipefail
ulimit -c 0
export PYTHONDONTWRITEBYTECODE=1
readonly root={shlex.quote(str(ROOT))}
readonly run={shlex.quote(str(run))}
readonly build={shlex.quote(str(build))}
readonly harness={shlex.quote(str(command))}
readonly parallel=8
cd "$root"
if [[ "${{SLURM_CPUS_PER_TASK:-0}}" -lt "$parallel" ]]; then
  echo "need at least $parallel CPUs, got ${{SLURM_CPUS_PER_TASK:-unset}}" >&2
  exit 2
fi
python3 "$harness" verify "$run"
readonly stamp="${{SLURM_JOB_ID:-manual}}.$$"
readonly log_tmp="$run/ctest.log.$stamp.tmp"
readonly junit_tmp="$run/junit.xml.$stamp.tmp"
readonly exit_tmp="$run/exit-code.txt.$stamp.tmp"
set +e
ctest --test-dir "$build" -j "$parallel" --no-tests=error \\
  --output-on-failure --output-junit "$junit_tmp" --output-log "$log_tmp"
ctest_status=$?
python3 "$harness" verify "$run"
verify_status=$?
set -e
mv "$log_tmp" "$run/ctest.log"
mv "$junit_tmp" "$run/junit.xml"
if [[ "$verify_status" -ne 0 ]]; then final_status=125; else final_status="$ctest_status"; fi
printf '%s\n' "$final_status" > "$exit_tmp"
mv "$exit_tmp" "$run/exit-code.txt"
exit "$final_status"
'''
    path = run / "run.sbatch"
    path.write_text(body)
    path.chmod(0o550)


def archived_source(source_campaign: Path, source: Path) -> tuple[dict, list[str]]:
    manifest_path = source_campaign / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("schema") not in ("worktree-archived-source-full-debug-v2", SCHEMA):
        raise ArchiveHarnessError("source campaign is not an archived full-Debug campaign")
    original_source = ROOT / manifest["source_snapshot"]
    original_archive = ROOT / manifest["source_archive"]
    if sha_file(original_archive) != manifest["source_archive_sha256"]:
        raise ArchiveHarnessError("source campaign archive changed")
    if tree_digest(original_source, manifest["mutable_source_roots"]) != manifest["source_snapshot_sha256"]:
        raise ArchiveHarnessError("source campaign snapshot changed")
    with tarfile.open(original_archive, "r") as archive:
        for member in archive.getmembers():
            pure = PurePosixPath(member.name)
            if pure.is_absolute() or ".." in pure.parts:
                raise ArchiveHarnessError(f"unsafe source archive member: {member.name}")
        archive.extractall(source)
    metadata = {
        "source_head": manifest["source_head"],
        "worktree_state_sha256": manifest["source_snapshot_sha256"],
        "source_authority": {"manifest": relative(manifest_path),
                             "manifest_sha256": sha_file(manifest_path),
                             "archive_sha256": manifest["source_archive_sha256"]},
    }
    return metadata, list(manifest["packaged_dependencies"])


def prepare(run: Path, build: Path, *, force: bool,
            source_campaign: Optional[Path] = None) -> None:
    if run.parent.resolve(strict=True) != RUNS.resolve(strict=True):
        raise ArchiveHarnessError(f"campaign is not a direct child of {RUNS}: {run}")
    if run.is_symlink():
        raise ArchiveHarnessError(f"refusing symlink campaign: {run}")
    if run.exists():
        if not force:
            raise ArchiveHarnessError(f"campaign exists; pass --force to replace it: {run}")
        shutil.rmtree(run)
    run.mkdir()
    source = run / "source"
    archive_path = run / "source.tar"
    if source_campaign is None:
        source.mkdir()
        entries = source_entries(run)
        before = entry_digest(entries)
        for name in entries:
            copy_entry(ROOT / name, source / name)
        dependencies = copy_dependencies(source)
        source_metadata = {
            "source_head": output("git", "rev-parse", "HEAD").decode().strip(),
            "worktree_state_sha256": before,
        }
    else:
        source_campaign = source_campaign.resolve(strict=True)
        if source_campaign == run or source_campaign.parent.resolve(strict=True) != RUNS.resolve(strict=True):
            raise ArchiveHarnessError("source campaign must be a different direct child of benchmarks/runs")
        source.mkdir()
        source_metadata, dependencies = archived_source(source_campaign, source)
    for relative_path in MUTABLE:
        (source / relative_path).mkdir(parents=True, exist_ok=True)
    if source_campaign is None and (source_entries(run) != entries or entry_digest(entries) != before):
        raise ArchiveHarnessError("worktree changed while snapshotting")
    canonical_archive(source, archive_path)

    BUILD_ROOT.mkdir(exist_ok=True)
    prepare_clean_build(build, BUILD_ROOT, force=force)
    run_logged(run / "configure.log", [
        "cmake", "-S", str(source), "-B", str(build), "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_COMPILER=/cm/local/apps/gcc/13.1.0/bin/gcc",
        "-DCMAKE_CXX_COMPILER=/cm/local/apps/gcc/13.1.0/bin/c++",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", "-DTACTIX_ENABLE_EVENT_TRACE=OFF",
        "-DTACTIX_ENABLE_PROOF_SHADOW=OFF", "-DTACTIX_ENABLE_RULE_JOURNAL=OFF",
    ])
    run_logged(run / "build.log", ["cmake", "--build", str(build), "-j", "8"])
    current = inventory(build)
    commands = validate_test_commands(current, build)
    write_batch(run, build)
    template = {
        "schema": TEMPLATE_SCHEMA,
        **source_metadata,
        "source_archive": relative(archive_path),
        "source_archive_sha256": sha_file(archive_path),
        "source_snapshot": relative(source),
        "pin": relative(build),
        "mutable_source_roots": [path.as_posix() for path in MUTABLE],
        "packaged_dependencies": dependencies,
        "expected_tests": len(current["tests"]),
        "validated_build_commands": len(commands),
        "build": {"type": "Debug", "clean": True, "event_trace": False,
                  "proof_shadow": False, "rule_journal": False,
                  "c_compiler": "/cm/local/apps/gcc/13.1.0/bin/gcc",
                  "cxx_compiler": "/cm/local/apps/gcc/13.1.0/bin/c++"},
        "resource_contract": {"partition": "cpu-dense-hwulab-q", "cpus_per_task": 8,
                              "ctest_parallel": 8, "memory_mib": 32768,
                              "wall_seconds": 28800},
    }
    (run / "manifest.template.json").write_text(json.dumps(template, indent=2, sort_keys=True) + "\n")
    finalize(run)


def finalize(run: Path) -> None:
    template_path = run / "manifest.template.json"
    output_path = run / "manifest.json"
    template = json.loads(template_path.read_text())
    if template.get("schema") != TEMPLATE_SCHEMA:
        raise ArchiveHarnessError("wrong Debug archive template schema")
    source = ROOT / template["source_snapshot"]
    build = ROOT / template["pin"]
    current = inventory(build)
    tests = current.get("tests", [])
    if len(tests) != template["expected_tests"]:
        raise ArchiveHarnessError("CTest inventory count changed")
    commands = validate_test_commands(current, build)
    if len(commands) != template["validated_build_commands"]:
        raise ArchiveHarnessError("validated build-command count changed")
    campaign_files = {template_path, run / "run.sbatch", Path(__file__).resolve(),
                      ROOT / "tools/bench/full_debug_archive.py"}
    artifacts = {build / "CMakeCache.txt", build / "compile_commands.json", build / "tactix", *commands}
    artifacts.update(build.rglob("CTestTestfile.cmake"))
    direct = set()
    for test in tests:
        for arg in test.get("command", []):
            candidate = Path(arg)
            if candidate.is_absolute() and candidate.is_file() and inside(candidate, source):
                direct.add(candidate)
    dependencies = packaged_dependency_paths(source, template["packaged_dependencies"])
    sets = {"campaign_files": campaign_files,
            "packaged_dependencies": dependencies,
            "direct_test_inputs": direct,
            "build_artifacts": artifacts}
    files = set().union(*sets.values())
    bindings = {relative(path): sha_file(path) for path in sorted(files, key=relative)}
    result = dict(template)
    result.update({
        "schema": SCHEMA,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "source_snapshot_sha256": tree_digest(source, template["mutable_source_roots"]),
        "tests": len(tests),
        "test_names": [test["name"] for test in tests],
        "ctest_inventory_sha256": canonical(current),
        "binding_sets": {name: sorted(relative(path) for path in paths) for name, paths in sets.items()},
        "bindings": {name: len(paths) for name, paths in sets.items()} | {"total": len(bindings)},
        "sha256": bindings,
    })
    temporary = output_path.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    temporary.replace(output_path)
    verify(run)
    print(f"prepared {run.relative_to(ROOT)} with {len(tests)} tests, "
          f"{len(commands)} executable commands, and {len(bindings)} bindings")


def verify(run: Path) -> None:
    manifest = json.loads((run / "manifest.json").read_text())
    if manifest.get("schema") != SCHEMA:
        raise ArchiveHarnessError("wrong Debug archive manifest schema")
    source = ROOT / manifest["source_snapshot"]
    archive_path = ROOT / manifest["source_archive"]
    build = ROOT / manifest["pin"]
    if sha_file(archive_path) != manifest["source_archive_sha256"]:
        raise ArchiveHarnessError("source archive changed")
    if tree_digest(source, manifest["mutable_source_roots"]) != manifest["source_snapshot_sha256"]:
        raise ArchiveHarnessError("immutable source inputs changed")
    for name in manifest["mutable_source_roots"]:
        if not (source / name).is_dir():
            raise ArchiveHarnessError(f"missing mutable output root: {name}")
    current = inventory(build)
    commands = validate_test_commands(current, build)
    if len(current.get("tests", [])) != manifest["tests"] or len(commands) != manifest["validated_build_commands"]:
        raise ArchiveHarnessError("CTest inventory or executable-command count changed")
    if [test["name"] for test in current["tests"]] != manifest["test_names"]:
        raise ArchiveHarnessError("CTest test names changed")
    if canonical(current) != manifest["ctest_inventory_sha256"]:
        raise ArchiveHarnessError("CTest inventory changed")
    flattened = [path for paths in manifest["binding_sets"].values() for path in paths]
    if len(flattened) != len(set(flattened)) or set(flattened) != set(manifest["sha256"]):
        raise ArchiveHarnessError("binding inventory is inconsistent")
    dependency_bindings = manifest["binding_sets"].get("packaged_dependencies")
    if dependency_bindings is not None:
        expected = sorted(relative(path) for path in
                          packaged_dependency_paths(source, manifest["packaged_dependencies"]))
        if dependency_bindings != expected:
            raise ArchiveHarnessError("packaged dependency bindings are inconsistent")
    for name, expected in manifest["sha256"].items():
        pure = PurePosixPath(name)
        if pure.is_absolute() or ".." in pure.parts or sha_file(ROOT.joinpath(*pure.parts)) != expected:
            raise ArchiveHarnessError(f"binding changed: {name}")
    print(f"verified archived source, {manifest['tests']} tests, "
          f"{len(commands)} executables, and {len(flattened)} bindings", flush=True)


def campaign_path(value: str) -> Path:
    path = (ROOT / value).resolve(strict=False) if not Path(value).is_absolute() else Path(value).resolve(strict=False)
    return path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("campaign", type=campaign_path)
    prepare_parser.add_argument("--build-name", required=True)
    prepare_parser.add_argument("--force", action="store_true")
    prepare_parser.add_argument("--source-campaign", type=campaign_path)
    verify_parser = commands.add_parser("verify")
    verify_parser.add_argument("campaign", type=campaign_path)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            if "/" in args.build_name or args.build_name in ("", ".", ".."):
                raise ArchiveHarnessError("build name must be one path component")
            prepare(args.campaign, BUILD_ROOT / args.build_name, force=args.force,
                    source_campaign=args.source_campaign)
        else:
            verify(args.campaign)
    except ArchiveHarnessError as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
