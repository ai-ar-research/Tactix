#!/usr/bin/env python3
"""Archive the campaigns behind the paper's evaluation into paper/logs/.

This ran once, in the development tree that holds the original campaign
directories; it is kept to document where every archive came from. Each
campaign becomes paper/logs/<experiment>/raw/<campaign>.tar.xz holding its
manifest, per-task result records, SLURM logs and runner scripts. Pinned
binaries, scratch space and the harness's bookkeeping directories are left
out.

    python3 paper/scripts/collect_logs.py --tactix-root DIR --special-root DIR
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

PAPER = Path(__file__).resolve().parents[1]

# experiment, tree, campaign directory relative to that tree
CAMPAIGNS = [
    ("tierd", "tactix", "benchmarks/runs/20261010-tierd-fastel"),
    ("tierd", "tactix", "benchmarks/runs/20261011-tierd-fastel-recheck-64g"),
    ("tierd", "tactix", "benchmarks/baselines/sc24-26-32g-5000s"),
    ("tierd", "tactix", "benchmarks/baselines/sc24-26-satsuma-fixed64-32g-8h"),
    ("tierd", "tactix", "benchmarks/baselines/sc24-26-recheck-64g-8h"),
    ("tierd", "tactix", "benchmarks/baselines/sc24-26-satsuma-recheck-64g-8h"),
    ("milestones", "tactix", "benchmarks/runs/20261011-tierd-noproof-1-tierA-dcc3ecbd6"),
    ("milestones", "tactix", "benchmarks/runs/20261011-tierd-noproof-2-tierB14-546298924"),
    ("milestones", "tactix", "benchmarks/runs/20261011-tierd-noproof-3-tierC-6f44f23d6"),
    ("milestones", "tactix", "benchmarks/runs/20261011-tierd-noproof-4-rank1-50-f50e46c0c"),
    ("milestones", "tactix", "benchmarks/runs/20261011-tierd-noproof-5-wv100-0a9063387"),
    ("interface", "tactix", "benchmarks/runs/20261015-120822-A-tactix-public"),
    ("interface", "tactix", "benchmarks/runs/20261015-public-port-v3-regression-b28c5"),
    ("interface", "tactix", "benchmarks/runs/20261015-prod-samewindow-regression-b28c5"),
    ("interface", "tactix", "benchmarks/runs/20261015-public-port-v3-regression-rank1-50"),
    ("interface", "tactix", "benchmarks/runs/20261015-prod-samewindow-regression-rank1-50"),
    ("interface", "tactix", "benchmarks/runs/20261015-public-port-v3-regression-wv100"),
    ("interface", "tactix", "benchmarks/runs/20261015-prod-samewindow-regression-wv100"),
    ("cruxmiter", "special", "benchmarks/runs/20261010-213743-crux-fixed-family-verify"),
    ("chvatal", "special", "benchmarks/runs/20261015-141329-chvatal-population-kissat"),
    ("chvatal", "special", "benchmarks/runs/20261015-144648-chvatal-band-baselines"),
    ("chvatal", "special", "benchmarks/runs/20261015-144649-chvatal-band-satsuma"),
    ("chvatal", "special", "benchmarks/runs/20261015-145107-chvatal-band-candidates"),
    ("chvatal", "special", "benchmarks/runs/20261015-150949-chvatal-band-recheck48-1"),
    ("chvatal", "special", "benchmarks/runs/20261015-160036-chvatal-hard-recheck48-2"),
    ("chvatal", "special", "benchmarks/runs/20261015-171709-chvatal-hard-recheck48-3"),
    ("chvatal", "special", "benchmarks/runs/20261015-173818-chvatal-hard-satsuma-recheck64"),
]
# Small inputs that define an experiment's instance set.
EXTRA_FILES = [
    ("chvatal", "special", "corpus/chvatal/paper-hard.json"),
    ("chvatal", "special", "corpus/chvatal/paper-band.json"),
    ("chvatal", "special", "corpus/chvatal/population.json"),
]
EXCLUDE = ("pins", "scratch", "provisional", "claims", "pending")


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--tactix-root", type=Path, required=True)
    parser.add_argument("--special-root", type=Path, required=True)
    args = parser.parse_args()
    roots = {"tactix": args.tactix_root, "special": args.special_root}
    index = []
    for experiment, tree, relative in CAMPAIGNS:
        source = roots[tree] / relative
        name = source.name
        target = PAPER / "logs" / experiment / "raw" / f"{name}.tar.xz"
        target.parent.mkdir(parents=True, exist_ok=True)
        command = ["tar", "-C", str(source.parent), "-cJf", str(target)]
        command += [f"--exclude={name}/{part}" for part in EXCLUDE]
        command += ["--exclude=*.proof", "--exclude=*.drat", "--exclude=*.dsr", name]
        subprocess.run(command, check=True, env={"XZ_OPT": "-9", "PATH": "/usr/bin:/bin"})
        index.append({"experiment": experiment, "archive": str(target.relative_to(PAPER)),
                      "source": relative, "sha256": sha256(target),
                      "bytes": target.stat().st_size})
        print(f"{experiment:11} {target.stat().st_size / 1e6:7.2f} MB  {name}")
    for experiment, tree, relative in EXTRA_FILES:
        source = roots[tree] / relative
        target = PAPER / "logs" / experiment / Path(relative).name
        target.write_bytes(source.read_bytes())
        index.append({"experiment": experiment, "archive": str(target.relative_to(PAPER)),
                      "source": relative, "sha256": sha256(target),
                      "bytes": target.stat().st_size})
    (PAPER / "logs" / "index.json").write_text(json.dumps(index, indent=1) + "\n")


if __name__ == "__main__":
    main()
