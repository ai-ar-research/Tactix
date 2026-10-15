#!/usr/bin/env python3
"""Satsuma+Kissat arm with certificate checking for the Chvatal paper runs.

tools/bench/run.py cannot check this solver's proof: the pinned SC2026 runner
writes a binary DSR stream that needs the frozen baseline route
dsr-trim (long64-fixed) -> dsr-compress -d -> srcheck.  This runner times the
solve exactly as run.py does (wall clock of the solver command, proof written),
then checks the certificate in a separate, separately timed stage.

  submit: chvatal_satsuma.py --instances LIST --timeout S --label NAME
  task:   chvatal_satsuma.py --task RUN_DIR INDEX   (run by the array job)
"""

import argparse
import datetime
import hashlib
import json
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RUNS = os.path.join(ROOT, "benchmarks", "runs")
SOLVER = os.path.join(ROOT, "tools/solvers/variants/satsuma-kissat-sc2026/"
                      "run_satsuma_kissat.sh")
ROUTE = os.path.join(ROOT, "tools/checkers/variants/satsuma-route")
CHECKERS = {name: os.path.join(ROUTE, name)
            for name in ("dsr-trim", "dsr-compress", "srcheck")}


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run_limited(cmd, limit, stdout_path):
    started = time.monotonic()
    with open(stdout_path, "w") as out:
        proc = subprocess.Popen(cmd, stdout=out, stderr=subprocess.STDOUT,
                                start_new_session=True)
        try:
            proc.wait(timeout=limit)
            timed_out = False
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
            timed_out = True
    return proc.returncode, time.monotonic() - started, timed_out


def task(run_dir, index):
    manifest = json.load(open(os.path.join(run_dir, "manifest.json")))
    rel = manifest["instances"][index]
    cnf = os.path.join(ROOT, rel)
    timeout = manifest["timeout"]
    check_timeout = manifest["check_timeout"]
    scratch_root = os.environ.get("SLURM_TMPDIR") or tempfile.gettempdir()
    work = tempfile.mkdtemp(prefix="chvatal-satsuma-", dir=scratch_root)
    result = {"instance": rel, "solver": "satsuma-kissat", "verdict": "UNKNOWN",
              "time": timeout, "solved": False, "proof_ok": None,
              "proof_bytes": None, "proof_check_time": None,
              "proof_route": "dsr-trim-compress-srcheck",
              "proof_msg": None, "node": socket.gethostname()}
    try:
        proof_dir = os.path.join(work, "proof")
        os.makedirs(proof_dir)
        stdout = os.path.join(work, "solver.out")
        code, elapsed, timed_out = run_limited(
            [SOLVER, cnf, proof_dir], timeout, stdout)
        text = open(stdout, errors="replace").read()
        if not timed_out and "s UNSATISFIABLE" in text:
            result.update(verdict="UNSAT", time=round(elapsed, 3), solved=True)
        elif not timed_out and "s SATISFIABLE" in text:
            result.update(verdict="SAT", time=round(elapsed, 3), solved=True,
                          proof_msg="SAT model not checked by this runner")
        if result["verdict"] == "UNSAT":
            proof = os.path.join(proof_dir, "proof.out")
            result["proof_bytes"] = os.path.getsize(proof)
            blsr = os.path.join(work, "proof.binary.lsr")
            lsr = os.path.join(work, "proof.lsr")
            stages = [[CHECKERS["dsr-trim"], cnf, proof, blsr],
                      [CHECKERS["dsr-compress"], "-d", blsr, lsr],
                      [CHECKERS["srcheck"], cnf, lsr]]
            started = time.monotonic()
            ok = True
            for number, cmd in enumerate(stages, 1):
                remaining = check_timeout - (time.monotonic() - started)
                out = os.path.join(work, f"check-{number}.out")
                code, _, expired = run_limited(cmd, max(1, remaining), out)
                if expired or code != 0:
                    ok = False
                    result["proof_msg"] = (f"stage {number} "
                                           f"{'timed out' if expired else f'exit {code}'}")
                    break
                if number == 1:
                    os.unlink(proof)
                if number == 2:
                    os.unlink(blsr)
            if ok:
                verdict = open(out, errors="replace").read()
                ok = "s VERIFIED UNSAT" in verdict
                result["proof_msg"] = "VERIFIED" if ok else "srcheck did not verify"
            result["proof_ok"] = ok
            result["proof_check_time"] = round(time.monotonic() - started, 3)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    os.makedirs(os.path.join(run_dir, "results"), exist_ok=True)
    with open(os.path.join(run_dir, "results", f"{index:05d}.json"), "w") as out:
        json.dump(result, out, indent=1)


def submit(args):
    rels = [line.strip() for line in open(args.instances) if line.strip()]
    stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = os.path.join(RUNS, f"{stamp}-{args.label}")
    os.makedirs(os.path.join(run_dir, "logs"))
    cpus = 2 * max(1, -(-args.mem // 8))
    manifest = {
        "instances": rels, "timeout": args.timeout,
        "check_timeout": args.check_timeout, "partition": args.partition,
        "memory_gib": args.mem, "cpus_per_task": cpus,
        "solver": SOLVER, "solver_sha256": digest(SOLVER),
        "checkers": {k: {"path": v, "sha256": digest(v)} for k, v in CHECKERS.items()},
        "input_sha256": {rel: digest(os.path.join(ROOT, rel)) for rel in rels},
        "wrapper_sha256": digest(__file__),
        "git": subprocess.run(["git", "-C", ROOT, "rev-parse", "--short", "HEAD"],
                              capture_output=True, text=True).stdout.strip(),
    }
    with open(os.path.join(run_dir, "manifest.json"), "w") as out:
        json.dump(manifest, out, indent=1)
    hours = int((args.timeout + args.check_timeout) * 1.1 // 3600) + 1
    script = os.path.join(run_dir, "array.sbatch")
    with open(script, "w") as out:
        out.write(f"""#!/usr/bin/env bash
#SBATCH --partition={args.partition}
#SBATCH --job-name=chvatal-satsuma-{args.label}
#SBATCH --nodes=1 --ntasks=1 --cpus-per-task={cpus}
#SBATCH --mem={args.mem}G
#SBATCH --time={hours:02d}:00:00
#SBATCH --array=0-{len(rels) - 1}
#SBATCH --output={run_dir}/logs/task-%a.log
exec /cm/local/apps/python3/bin/python3 {os.path.abspath(__file__)} --task {run_dir} $SLURM_ARRAY_TASK_ID
""")
    job = subprocess.run(["sbatch", "--parsable", script], check=True,
                         capture_output=True, text=True).stdout.strip()
    print(f"submitted array {job}: {run_dir}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--task", nargs=2, metavar=("RUN_DIR", "INDEX"))
    parser.add_argument("--instances")
    parser.add_argument("--timeout", type=float)
    parser.add_argument("--check-timeout", type=float, default=40000.0)
    parser.add_argument("--partition", default="cpu-q")
    parser.add_argument("--mem", type=int, default=48)
    parser.add_argument("--label", default="chvatal-satsuma")
    args = parser.parse_args()
    if args.task:
        task(args.task[0], int(args.task[1]))
    else:
        if not args.instances or not args.timeout:
            raise SystemExit("--instances and --timeout are required")
        submit(args)


if __name__ == "__main__":
    main()
