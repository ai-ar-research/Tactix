#!/usr/bin/env python3
"""Prepare once-only, independently checked reruns for missing cpu-q Tier-D rows.

Run only after the original array has left Slurm. Its results stay in a
separate campaign and are merged by cms_tierd_cpuq.py --summary.
"""

import argparse
import copy
import datetime as dt
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from frozen_baseline import atomic_json, sha256

ROOT = Path(__file__).resolve().parents[2]


def result_inventory(paths):
    """Hash an ordered set of immutable published result files."""
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(f"{path.name} {sha256(path)}\n".encode())
    return digest.hexdigest()


def array_expression(indices):
    groups = []
    start = previous = indices[0]
    for index in indices[1:]:
        if index == previous + 1:
            previous = index
            continue
        groups.append(str(start) if start == previous else f"{start}-{previous}")
        start = previous = index
    groups.append(str(start) if start == previous else f"{start}-{previous}")
    return ",".join(groups)


def workspace_cleanup_prologue(campaign):
    """Shell fallback for scratch left after an interrupted pinned runner."""
    return f"""set -uo pipefail
case "${{SLURM_ARRAY_TASK_ID:-}}" in
  ''|*[!0-9]*) echo 'invalid SLURM_ARRAY_TASK_ID' >&2; exit 98 ;;
esac
printf -v task '%05d' "$SLURM_ARRAY_TASK_ID"
scratch='{campaign}/scratch/'"$task"
cleanup_scratch() {{
  rc=$?
  trap - EXIT
  delay=1
  for attempt in 1 2 3 4 5 6; do
    rm -rf -- "$scratch" || true
    test ! -e "$scratch" && exit "$rc"
    sleep "$delay"
    delay=$((delay * 2))
  done
  echo "workspace scratch cleanup failed: $scratch" >&2
  exit 97
}}
trap cleanup_scratch EXIT
"""


def require_terminal(base):
    launch = json.loads((base / "launch.json").read_text())
    job = str(launch["array_job_id"])
    if not re.fullmatch(r"[0-9]+", job):
        raise RuntimeError("invalid original array job ID")
    queue = subprocess.run(["squeue", "-h", "-j", job, "-o", "%i"],
                           capture_output=True, text=True, timeout=120)
    if queue.returncode and "Invalid job id" not in queue.stderr + queue.stdout:
        raise RuntimeError("cannot determine original array state")
    if queue.stdout.strip():
        raise RuntimeError("original Tier-D array is still live")
    return job


def require_job_terminal(job):
    if not re.fullmatch(r"[0-9]+", str(job)):
        raise RuntimeError("invalid prior array job ID")
    queue = subprocess.run(["squeue", "-h", "-j", str(job), "-o", "%i"],
                           capture_output=True, text=True, timeout=120)
    if queue.returncode and "Invalid job id" not in queue.stderr + queue.stdout:
        raise RuntimeError(f"cannot determine state of prior array {job}")
    if queue.stdout.strip():
        raise RuntimeError(f"prior array {job} is still live")


def prior_launches(repair):
    paths = [repair / "launch.json"]
    paths.extend(sorted((repair / "rounds").glob("*/launch.json")))
    launches = []
    for path in paths:
        if path.is_file():
            value = json.loads(path.read_text())
            job = str(value.get("array_job_id", ""))
            launches.append({"path": str(path.relative_to(repair)),
                             "array_job_id": job})
    if not launches:
        raise RuntimeError("repair campaign has no submitted array")
    return launches


def prepare_continuation(repair, *, throttle=125,
                         terminal_check=require_job_terminal):
    """Prepare one nonoverlapping retry of still-unpublished repair rows.

    The caller submits the generated script through submit_full_tierd.py with
    --round-dir, which gives the retry its own read-only storage monitor. No retry can
    coexist with an earlier array.
    """
    repair = repair.resolve()
    manifest_path = repair / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    declared = manifest.get("repair_of", {}).get("missing_indices")
    if (manifest.get("kind") not in {
                "cryptominisat-tierd-cpuq-workspace-repair-v1",
                "cryptominisat-tierd-cpuq-workspace-repair-v2"} or
            not isinstance(declared, list) or not declared or
            sorted(set(declared)) != declared or
            manifest.get("certificate_policy", {}).get("retention") != "none" or
            manifest.get("certificate_policy", {}).get("storage") !=
            "workspace-ephemeral" or
            manifest.get("slurm", {}).get("array_throttle") != throttle):
        raise RuntimeError("campaign is not the pinned workspace repair contract")
    if ("workspace_quota_stop_gib" in manifest.get("limits", {}) or
            "workspace-quota" in manifest.get("provenance", {})):
        raise RuntimeError(
            "legacy workspace campaign has an active quota killer; prepare a "
            "successor repair instead of continuing it")
    launches = prior_launches(repair)
    for launch in launches:
        terminal_check(launch["array_job_id"])
    pending = sorted((repair / "pending").glob("*.json"))
    scratch = list((repair / "scratch").iterdir())
    if pending or scratch:
        raise RuntimeError(f"repair cleanup incomplete: pending={len(pending)}, "
                           f"scratch={len(scratch)}")
    published = {int(path.stem) for path in (repair / "results").glob("*.json")}
    declared_set = set(declared)
    if published - declared_set:
        raise RuntimeError("repair published a row outside its declared scope")
    missing = sorted(declared_set - published)
    if not missing:
        raise RuntimeError("repair campaign has no unpublished rows")

    rounds = repair / "rounds"
    rounds.mkdir(exist_ok=True)
    existing = sorted(path for path in rounds.iterdir() if path.is_dir())
    ordinal = len(existing) + 2
    round_dir = rounds / f"{ordinal:04d}"
    if round_dir.exists():
        raise RuntimeError(f"continuation round already exists: {round_dir}")

    stale_claims = []
    for index in missing:
        claim = repair / "claims" / f"{index:05d}.json"
        if claim.is_file():
            stale_claims.append({"task": index, "sha256": sha256(claim)})
    round_dir.mkdir()
    (round_dir / "logs").mkdir()
    script = round_dir / "array.sbatch"
    runner = repair / "pins/frozen_baseline.py"
    expression = array_expression(missing)
    script.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=cms-tierd-repair-r{ordinal}
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=14:15:00
#SBATCH --array={expression}%{throttle}
#SBATCH --no-requeue
#SBATCH --output={round_dir}/logs/task-%a.log
export TACTIX_REPO_ROOT='{ROOT}'
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR='{repair}/scratch'
{workspace_cleanup_prologue(repair)}python3 '{runner}' --campaign '{repair}' --run-one "$SLURM_ARRAY_TASK_ID"
""")
    record = {
        "schema": "cms-tierd-workspace-repair-round-v1",
        "round": ordinal,
        "created_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "campaign": repair.name,
        "campaign_manifest_sha256": sha256(manifest_path),
        "prior_launches": launches,
        "published_before": len(published),
        "missing_indices": missing,
        "array_throttle": throttle,
        "stale_claims_removed": stale_claims,
        "array_sbatch": {"path": "array.sbatch", "sha256": sha256(script)},
        "cleanup_precondition": {"pending_journals": 0, "scratch_directories": 0},
    }
    atomic_json(round_dir / "round.json", record, exclusive=True)
    for item in stale_claims:
        (repair / "claims" / f"{item['task']:05d}.json").unlink()
    print(f"prepared continuation round {ordinal}: {len(missing)} missing rows; "
          f"removed {len(stale_claims)} stale claims; throttle={throttle}")
    return round_dir


def prepare(base, repair, *, workspace=False, throttle=8, prior_repairs=()):
    base, repair = base.resolve(), repair.resolve()
    prior_repairs = [Path(path).resolve() for path in prior_repairs]
    if repair.exists():
        raise RuntimeError(f"repair campaign already exists: {repair}")
    job = require_terminal(base)
    original = json.loads((base / "manifest.json").read_text())
    if (original.get("kind") not in {
                "cryptominisat-tierd-cpuq-anonymous-baseline-v2",
                "cryptominisat-tierd-cpuq-workspace-baseline-v1"} or
            len(original.get("instances", [])) != 1166 or
            len(original.get("tasks", [])) != 1166 or
            original.get("certificate_policy", {}).get("retention") != "none"):
        raise RuntimeError("original campaign is not the frozen cpu-q zero-retention rerun")
    pending = sorted((base / "pending").glob("*.json"))
    if pending:
        raise RuntimeError(f"resolve {len(pending)} pending cleanup journals before repair")
    base_published = {int(path.stem)
                      for path in (base / "results").glob("*.json")}
    missing = set(range(1166)) - base_published
    prior_records = []
    for prior in prior_repairs:
        prior_manifest_path = prior / "manifest.json"
        prior_manifest = json.loads(prior_manifest_path.read_text())
        prior_base = prior_manifest.get("repair_of", {})
        if (not workspace or
                prior_manifest.get("kind") not in {
                    "cryptominisat-tierd-cpuq-workspace-repair-v1",
                    "cryptominisat-tierd-cpuq-workspace-repair-v2"} or
                prior_base.get("campaign") != base.name or
                prior_base.get("manifest_sha256") != sha256(base / "manifest.json")):
            raise RuntimeError("prior repair does not belong to the frozen base")
        for launch in prior_launches(prior):
            require_job_terminal(launch["array_job_id"])
        if (list((prior / "pending").glob("*.json")) or
                list((prior / "scratch").iterdir())):
            raise RuntimeError("prior repair cleanup is incomplete")
        prior_paths = sorted((prior / "results").glob("*.json"))
        prior_indices = {int(path.stem) for path in prior_paths}
        if prior_indices - missing:
            raise RuntimeError("prior repairs overlap or publish outside missing base rows")
        for name in ("solver-binary", "frat-xor", "cake-xlrup", "checkmodel"):
            if (prior_manifest.get("provenance", {}).get(name, {}).get("sha256") !=
                    original["provenance"][name]["sha256"]):
                raise RuntimeError(f"prior repair changed checked pin: {name}")
        prior_records.append({
            "campaign": prior.name,
            "manifest_sha256": sha256(prior_manifest_path),
            "published_indices": sorted(prior_indices),
            "results_inventory_sha256": result_inventory(prior_paths),
        })
        missing -= prior_indices
    missing = sorted(missing)
    if not missing:
        raise RuntimeError("base and prior repairs have no missing result rows")
    pin_names = ["solver", "frat_xor", "cake_xlrup", "checkmodel",
                 "modelcheck.py", "libcadiback.so.0", "libcadical.so.3",
                 "libcryptominisat5.so.5.14"]
    if "anonymous-scratch" in original.get("provenance", {}):
        pin_names.append("anonymous_scratch.py")
    for name in pin_names:
        source = base / "pins" / name
        if not source.is_file():
            raise RuntimeError(f"missing original pin: {name}")
    provenance_names = ["solver-binary", "frat-xor", "cake-xlrup", "checkmodel",
                        "modelcheck.py", "frozen-baseline"]
    if "anonymous-scratch" in original.get("provenance", {}):
        provenance_names.append("anonymous-scratch")
    pinned = [original["provenance"][name] for name in provenance_names]
    pinned.extend(original["provenance"]["shared-libraries"])
    for pin in pinned:
        if sha256(base / pin["path"]) != pin["sha256"]:
            raise RuntimeError(f"original pin changed: {pin['path']}")
    repair.mkdir(parents=True)
    for directory in ("results", "pending", "claims", "scratch", "logs"):
        (repair / directory).mkdir()
    shutil.copytree(base / "pins", repair / "pins")
    runner = repair / "pins/frozen_baseline.py"
    shutil.copy2(ROOT / "tools/bench/frozen_baseline.py", runner)
    if workspace:
        (repair / "pins/workspace_quota.py").unlink(missing_ok=True)
    wrapper = repair / "pins/cms-wrapper"
    wrapper.chmod(0o755)
    wrapper.write_text(
        "#!/usr/bin/env bash\nset -euo pipefail\n"
        f"export LD_LIBRARY_PATH='{repair}/pins':${{LD_LIBRARY_PATH:-}}\n"
        f"exec '{repair}/pins/solver' --zero-exit-status --verb 0 \"$1\" \"$2\"\n")
    wrapper.chmod(0o555)
    manifest = copy.deepcopy(original)
    manifest["kind"] = (("cryptominisat-tierd-cpuq-workspace-repair-v2"
                         if prior_records else
                         "cryptominisat-tierd-cpuq-workspace-repair-v1")
                        if workspace else "cryptominisat-tierd-cpuq-repair-v1")
    manifest["campaign"] = repair.name
    manifest["created_utc"] = dt.datetime.now(dt.timezone.utc).isoformat()
    manifest["repair_of"] = {
        "campaign": base.name, "manifest_sha256": sha256(base / "manifest.json"),
        "array_job_id": job, "missing_indices": missing,
        "selection": ("base and predecessor arrays terminal; absent published "
                      "result only" if prior_records else
                      "original array terminal; absent published result only")}
    if prior_records:
        manifest["repair_of"]["prior_repairs"] = prior_records
    if workspace:
        manifest["certificate_policy"]["storage"] = "workspace-ephemeral"
        manifest["certificate_policy"]["terminal_cleanup"] = (
            "bounded-nfs-retry-in-finally-before-result-publication")
        manifest["limits"].pop("workspace_quota_stop_gib", None)
        manifest["limits"].pop("workspace_quota_poll_seconds", None)
        manifest["provenance"].pop("workspace-quota", None)
    else:
        manifest["limits"]["node_scratch_ledger_slots"] = 1
        manifest["slurm"]["node_scratch_participants"] = 1
    manifest["slurm"]["array_throttle"] = throttle
    # cpu-q caps allocations at 52 CPUs on 64-CPU nodes, so Slurm rejects
    # --exclusive there. Workspace repairs use the array throttle and
    # campaign-local scratch; legacy anonymous repairs retain their
    # one-participant scratch ledger.
    manifest["provenance"]["frozen-baseline"]["sha256"] = sha256(runner)
    manifest["provenance"]["cryptominisat-5.14.7"]["sha256"] = sha256(wrapper)
    atomic_json(repair / "manifest.json", manifest, exclusive=True)
    expression = array_expression(missing)
    cleanup = workspace_cleanup_prologue(repair) if workspace else ""
    launch = (f"python3 '{runner}' --campaign '{repair}' --run-one "
              '"$SLURM_ARRAY_TASK_ID"' if workspace else
              f"exec python3 '{runner}' --campaign '{repair}' --run-one "
              '"$SLURM_ARRAY_TASK_ID"')
    (repair / "array.sbatch").write_text(f"""#!/usr/bin/env bash
#SBATCH --partition=cpu-q
#SBATCH --job-name=cms-tierd-repair
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=4
#SBATCH --mem=32G
#SBATCH --time=14:15:00
#SBATCH --array={expression}%{throttle}
#SBATCH --no-requeue
#SBATCH --output={repair}/logs/task-%a.log
export TACTIX_REPO_ROOT='{ROOT}'
export PYTHONDONTWRITEBYTECODE=1
export TMPDIR='{repair}/scratch'
{cleanup}{launch}
""")
    print(f"prepared {repair}: {len(missing)} missing rows; "
          f"storage={'workspace' if workspace else 'anonymous-node-local'}; "
          f"throttle={throttle}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path)
    parser.add_argument("--repair", type=Path)
    parser.add_argument("--continue-campaign", type=Path)
    parser.add_argument("--prior-repair", type=Path, action="append", default=[])
    parser.add_argument("--workspace", action="store_true")
    parser.add_argument("--throttle", type=int, default=8)
    args = parser.parse_args()
    if args.throttle < 1 or args.throttle > 125:
        parser.error("--throttle must be between 1 and 125")
    if args.continue_campaign:
        if args.base or args.repair or args.workspace or args.prior_repair:
            parser.error("--continue-campaign cannot be combined with preparation options")
        if args.throttle != 125:
            parser.error("workspace continuation preserves the 125-task throttle")
        prepare_continuation(args.continue_campaign, throttle=args.throttle)
        return
    if not args.base or not args.repair:
        parser.error("--base and --repair are required for a new repair")
    if args.prior_repair and not args.workspace:
        parser.error("successor repairs require --workspace")
    if not args.workspace and args.throttle != 8:
        parser.error("non-workspace repair uses the proven eight-task throttle")
    prepare(args.base, args.repair, workspace=args.workspace,
            throttle=args.throttle, prior_repairs=args.prior_repair)


if __name__ == "__main__":
    main()
