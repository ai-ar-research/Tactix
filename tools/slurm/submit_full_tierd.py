#!/usr/bin/env python3
"""Submit a full Tier-D array with bounded concurrency and read-only monitoring."""

import argparse
import datetime as dt
import hashlib
import json
import re
import shutil
import subprocess
from pathlib import Path

from quota_governor import read_usage

MONITOR = Path(__file__).with_name("quota_monitor.py")


def atomic_json(path, value):
    temp = path.with_name("." + path.name + ".tmp")
    temp.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n")
    temp.replace(path)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def array_indices(script, manifest, expected_indices=None):
    match = re.search(r"^#SBATCH --array=([0-9,-]+)(?:%([0-9]+))?$", script, re.M)
    if not match:
        raise RuntimeError("array must declare numeric Tier-D task IDs")
    expression, throttle = match.groups()
    indices = []
    for item in expression.split(","):
        bounds = item.split("-")
        if len(bounds) == 1:
            indices.append(int(bounds[0]))
        elif len(bounds) == 2 and int(bounds[0]) <= int(bounds[1]):
            indices.extend(range(int(bounds[0]), int(bounds[1]) + 1))
        else:
            raise RuntimeError("invalid array task range")
    if (not indices or len(indices) != len(set(indices)) or
            min(indices) < 0 or max(indices) >= 1166):
        raise RuntimeError("array task IDs must be unique and within Tier D")
    repair = manifest.get("repair_of")
    if expected_indices is not None:
        if sorted(indices) != sorted(expected_indices):
            raise RuntimeError("continuation array does not match its prepared missing rows")
    elif repair:
        if sorted(indices) != sorted(repair.get("missing_indices", [])):
            raise RuntimeError("repair array does not match its declared missing rows")
    elif expression != "0-1165":
        raise RuntimeError("full Tier-D array must declare all 1,166 tasks")
    if throttle is None or not 1 <= int(throttle) <= 125:
        raise RuntimeError("Tier-D arrays require a throttle in 1..125")
    return indices


def array_partition(script):
    match = re.search(r"^#SBATCH --partition=([\w-]+)$", script, re.M)
    if not match:
        raise RuntimeError("array must declare its partition")
    if match.group(1) != "cpu-q":
        raise RuntimeError("full Tier-D arrays must use cpu-q")
    return match.group(1)


def workspace_contract(campaign, script, manifest):
    """Require full-run transient files to live under the campaign tree."""
    lowered = script.lower()
    if ("/tmp" in lowered or "slurm_tmpdir" in lowered or
            "scratchreserve" in lowered or "scratch-reserve" in lowered):
        raise RuntimeError("full Tier-D array may not use node-local or scratch-reserve storage")
    storage = manifest.get("certificate_policy", {}).get("storage")
    declared = manifest.get("scratch_directory")
    if storage == "workspace-ephemeral":
        return
    if declared:
        scratch = Path(declared).resolve()
        try:
            scratch.relative_to(campaign)
        except ValueError as error:
            raise RuntimeError("scratch directory must be inside the campaign") from error
        return
    raise RuntimeError("full Tier-D campaign lacks a campaign-local scratch contract")


def prior_job_is_live(job):
    check = subprocess.run(["squeue", "-h", "-j", str(job), "-o", "%i"],
                           capture_output=True, text=True, timeout=120)
    if check.returncode and "Invalid job id" not in check.stderr + check.stdout:
        raise RuntimeError(f"cannot determine state of prior array {job}")
    return bool(check.stdout.strip())


def submit(campaign, array, round_dir=None):
    campaign, array = campaign.resolve(), array.resolve()
    round_record = None
    expected_indices = None
    if round_dir is not None:
        round_dir = round_dir.resolve()
        if round_dir.parent != campaign / "rounds":
            raise RuntimeError("continuation directory must be directly under campaign/rounds")
        round_path = round_dir / "round.json"
        round_record = json.loads(round_path.read_text())
        manifest_path = campaign / "manifest.json"
        if (round_record.get("schema") != "cms-tierd-workspace-repair-round-v1" or
                round_record.get("campaign") != campaign.name or
                round_record.get("campaign_manifest_sha256") != sha256(manifest_path)):
            raise RuntimeError("continuation round does not bind this campaign")
        expected_indices = round_record.get("missing_indices")
        if not isinstance(expected_indices, list) or not expected_indices:
            raise RuntimeError("continuation round has no missing rows")
        for prior in round_record.get("prior_launches", []):
            if prior_job_is_live(prior.get("array_job_id")):
                raise RuntimeError("a prior repair array is still live")
        declared = set(json.loads(manifest_path.read_text()).get(
            "repair_of", {}).get("missing_indices", []))
        published = {int(path.stem) for path in
                     (campaign / "results").glob("*.json")}
        if sorted(declared - published) != expected_indices:
            raise RuntimeError("published result set changed after continuation preparation")
        launch = round_dir / "launch.json"
        monitor = round_dir / "storage-monitor"
    else:
        launch = campaign / "launch.json"
        monitor = campaign / "storage-monitor"
    if launch.exists():
        raise RuntimeError(f"already submitted: {launch}")
    script = array.read_text()
    partition = array_partition(script)
    manifest = json.loads((campaign / "manifest.json").read_text())
    if round_record is None and manifest.get("recheck_source"):
        matrix = campaign / "matrix.tsv"
        count = max(0, len(matrix.read_text().splitlines()) - 1)
        if count == 0:
            raise RuntimeError("Tier-D recheck campaign has no matrix rows")
        expected_indices = list(range(count))
    indices = array_indices(script, manifest, expected_indices)
    workspace_contract(campaign, script, manifest)
    if re.search(r"^#SBATCH --(?:nodelist|constraint|exclude)", script, re.M):
        raise RuntimeError("unexpected node placement constraint")
    usage, source = read_usage("hwu")
    if usage is None:
        raise RuntimeError("home quota usage unavailable; cannot pass prelaunch storage gate")
    if usage * 2**30 >= 700_000_000_000:
        raise RuntimeError(f"prelaunch 700 GB storage gate failed: {usage:.1f} GiB via {source}")
    if monitor.exists():
        raise RuntimeError(f"storage monitor already exists: {monitor}")
    monitor.mkdir()
    shutil.copy2(MONITOR, monitor / "quota_monitor.py")
    batch = monitor / "monitor.sbatch"
    batch.write_text(f"""#!/usr/bin/env bash
#SBATCH --partition={partition}
#SBATCH --job-name=tierd-storage-monitor
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task=1
#SBATCH --mem=2G
#SBATCH --time=7-00:00:00
#SBATCH --output={monitor}/monitor.out
export PYTHONDONTWRITEBYTECODE=1
exec python3 '{monitor}/quota_monitor.py' --dir '{monitor}' --interval 600 --job ARRAY_JOB_ID --user hwu
""")
    array_job = subprocess.run(["sbatch", "--parsable", str(array)],
                               capture_output=True, text=True, check=True).stdout.strip().split(";")[0]
    if not re.fullmatch(r"\d+", array_job):
        raise RuntimeError(f"unexpected sbatch job ID: {array_job!r}")
    batch.write_text(batch.read_text().replace("ARRAY_JOB_ID", array_job))
    pin_record = {
        "array.sbatch": sha256(array),
        "monitor.sbatch": sha256(batch),
        "quota_monitor.py": sha256(monitor / "quota_monitor.py"),
        "manifest.json": sha256(campaign / "manifest.json")}
    if round_record is not None:
        pin_record["round.json"] = sha256(round_dir / "round.json")
    atomic_json(monitor / "pins.json", pin_record)
    try:
        monitor_job = subprocess.run(["sbatch", "--parsable", str(batch)],
                                     capture_output=True, text=True, check=True).stdout.strip().split(";")[0]
        if not re.fullmatch(r"\d+", monitor_job):
            raise RuntimeError(f"unexpected monitor job ID: {monitor_job!r}")
    except Exception:
        atomic_json(launch, {"array_job_id": array_job, "monitor_job_id": None,
                             "state": "array-submitted-monitor-submission-failed"})
        raise
    record = {"array_job_id": array_job, "job_id": array_job,
              "monitor_job_id": monitor_job,
              "state": "submitted",
              "submitted_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
              "manifest_sha256": sha256(campaign / "manifest.json"),
              "prelaunch_usage_gib": usage, "prelaunch_usage_source": source,
              "array_sbatch": str(array.relative_to(campaign)),
              "monitor_sbatch": str(batch.relative_to(campaign)),
              "task_count": len(indices), "partition": partition,
              "concurrency_contract": "array throttle no larger than 125",
              "monitor_contract": "read-only quota sample every 600 seconds"}
    atomic_json(launch, record)
    print(json.dumps(record, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--array-sbatch", type=Path)
    parser.add_argument("--round-dir", type=Path)
    args = parser.parse_args()
    default_array = ((args.round_dir / "array.sbatch") if args.round_dir
                     else (args.campaign / "array.sbatch"))
    submit(args.campaign, args.array_sbatch or default_array, args.round_dir)


if __name__ == "__main__":
    main()
