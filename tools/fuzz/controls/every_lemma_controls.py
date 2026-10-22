#!/usr/bin/env python3
"""Causal controls for both DSR every-lemma audits (fuzz.run_case).

Witness-free native DSR is audited by forward drat-trim on its lossless DRAT
conversion, in addition to the backward drat-trim -> CakeLPR route; witnessed
DSR is audited by forward dsr-trim -> dsr-compress -> Trestle in addition to
the backward reference pipeline. The controls grade fixed certificates through
the harness's own run_case and its configured checkers (fuzz.CHECKER_DIR):

  f2-positive   a valid witness-free binary DSR proof (bundle 000508). The
                retained historical forward elaboration of it
                (f2-historical-forward.lsr, produced by checker 02824056) must
                be rejected by Trestle with `s INVALID`, so the control is
                non-vacuous without the historical executable. The configured
                forward dsr-trim must elaborate the same proof into an LSR that
                Trestle verifies, and the harness must accept it with forward
                drat-trim verifying every lemma, CakeLPR verifying the backward
                route and no dsr-trim stage run.
  f2w-positive  a valid witnessed proof (L1 bundle 003214). Trestle must
                reject its retained historical forward elaboration; the harness
                must accept it with both the backward reference pipeline and
                the forward every-lemma audit verifying at every stage.
  unused-bad    a proof whose invalid addition `3 4 0` is deleted unused before
                a valid refutation, in ASCII and binary DSR. Backward
                drat-trim and CakeLPR accept it; forward drat-trim rejects it
                (`s NOT VERIFIED`); the harness failure is exactly the forward
                audit reason.
  unused-bad-witnessed
                the same unused invalid addition written with an explicit
                witness (`3 4 3 0`), so it takes the witnessed route, in ASCII
                and binary DSR. Backward dsr-trim and Trestle accept it;
                forward dsr-trim rejects it; the harness failure is exactly the
                forward dsr-trim audit reason.
  valid         the same refutation without the bad lemma: no failure.

TACTIX_FUZZ_HISTORICAL_DSR_TRIM optionally names the historical checker for a
labelled diagnosis: its sha256 must be 02824056..., and on the F2 pair its
forward dsr-trim and dsr-compress must verify while Trestle rejects.

Exit 0 iff every control reached its intended oracle with its intended result.
"""
import hashlib
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
import fuzz  # noqa: E402
from proof_oracle import every_lemma_dsr_stages, proof_check_status  # noqa: E402

FIXTURES = os.path.join(HERE, "every-lemma")
WRAPPER = os.path.join(HERE, "emit-fixed-proof.sh")
# The first four clauses refute x1/x2; the last two keep neither literal of
# the unused (x3 or x4) addition a RAT pivot.
SMALL = "p cnf 5 6\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n-3 5 0\n-4 5 0\n"
FORWARD_REASON = ("proof", "every-lemma audit: forward drat-trim rejected "
                  "witness-free DSR")
FORWARD_DSR_REASON = ("proof", "every-lemma audit: dsr-trim did not verify "
                      "and elaborate DSR")
HISTORICAL_DSR_TRIM = ("028240565e37185ce6064fe0f044d28ed70fb8b8823e03fdb2cc0"
                       "2988cbacc74")
# The retained fixtures; a changed byte would make these controls about a
# different proof than the provenance records.
FIXTURE_SHA256 = {
    "f2-forward-elaboration.cnf":
        "e81598c4f4c80f6add03f8c82cc04c05a1fe7b40c4b8db641483fac8bbc59925",
    "f2-forward-elaboration.dsr":
        "93e5ad15bf98def9a43757ed827b864f00b292f54011943640e3c88263a3d44f",
    "f2-historical-forward.lsr":
        "3329b2e9d31ca55057838c087ed3896be652030dee787240656ca17ef81307ea",
    "f2w-witnessed.cnf":
        "2fab28864d19e911e718a2c800fbc1f1b7cb4263b1094124ad47a1c32df139c4",
    "f2w-witnessed.dsr":
        "ef2c118c0507e0c0a1317241fc3f26fa2d2ab3cdefe0fa8b1c2f30948b5a72d3",
    "f2w-historical-forward.lsr":
        "819bbe34cee9b855f7be5479582781338f14b2d63fa8266f3d664939737c994a",
}


def binary(lines):
    data = bytearray()
    for tag, lits in lines:
        data.append(tag)
        for lit in lits + [0]:
            value = 2 * abs(lit) + (lit < 0) if lit else 0
            while value > 127:
                data.append(value & 127 | 128)
                value >>= 7
            data.append(value)
    return bytes(data)


def grade(cnf, certificate, workdir):
    case = os.path.join(workdir, "case")
    os.makedirs(case)
    return fuzz.run_case(cnf, [WRAPPER, "--dsr", certificate], None,
                         30, False, case)


def runs(outcome, predicate):
    return [r for r in outcome.checker_runs if predicate(r["command"])]


def is_forward_drat(c):
    return os.path.basename(c[0]) == "drat-trim" and "-f" in c


def is_backward_drat(c):
    return os.path.basename(c[0]) == "drat-trim" and "-L" in c


def sha256(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def stage_statuses(stages):
    """Run (name, command) stages in order; [(name, status, stdout)] up to and
    including the first stage that does not verify."""
    statuses = []
    for name, command in stages:
        c, o, e = fuzz.run_cmd(list(map(str, command)), 120)
        status = (fuzz.dsr_compress_status(c, o + "\n" + e)
                  if name == "dsr-compress"
                  else proof_check_status(name, c, o, e))
        statuses.append((name, status, o))
        if status != "verified":
            break
    return statuses


def forward_dsr(dsr_trim, cnf, proof, workdir):
    os.makedirs(workdir)
    return stage_statuses(every_lemma_dsr_stages(
        dsr_trim, fuzz.DSR_COMPRESS, fuzz.SR_CHECK, cnf, proof,
        os.path.join(workdir, "p.binary.lsr"), os.path.join(workdir, "p.lsr")))


def trestle(cnf, lsr):
    return stage_statuses([("srcheck", [fuzz.SR_CHECK, cnf, lsr])])[0]


def names_statuses(statuses):
    return [(name, status) for name, status, _ in statuses]


def main():
    problems = []

    def require(ok, what):
        print(("ok   " if ok else "FAIL ") + what)
        if not ok:
            problems.append(what)

    with tempfile.TemporaryDirectory(prefix="every-lemma-") as tmp:
        for name, digest in FIXTURE_SHA256.items():
            require(sha256(os.path.join(FIXTURES, name)) == digest,
                    f"fixture {name} matches its recorded sha256")
        fixture = {name: os.path.join(FIXTURES, name) for name in FIXTURE_SHA256}
        print(f"configured checkers: {fuzz.CHECKER_DIR}; dsr-trim sha256 "
              f"{sha256(fuzz.DSR_TRIM)}")

        # f2-positive
        cnf = fixture["f2-forward-elaboration.cnf"]
        dsr = fixture["f2-forward-elaboration.dsr"]
        name, status, stdout = trestle(cnf, fixture["f2-historical-forward.lsr"])
        require(status == "rejected" and "s INVALID" in stdout.splitlines(),
                f"f2-positive: Trestle rejects the historical forward "
                f"elaboration (non-vacuous) [{status}]")
        repaired = forward_dsr(fuzz.DSR_TRIM, cnf, dsr,
                               os.path.join(tmp, "f2-forward"))
        require(names_statuses(repaired) ==
                [("dsr-trim", "verified"), ("dsr-compress", "verified"),
                 ("srcheck", "verified")],
                f"f2-positive: configured forward dsr-trim elaboration "
                f"verifies through Trestle {names_statuses(repaired)}")
        out = grade(cnf, dsr, os.path.join(tmp, "f2"))
        require(out.verdict == "UNSAT" and out.failures == [],
                f"f2-positive: harness accepts, failures={out.failures}")
        forward = runs(out, is_forward_drat)
        require(len(forward) == 1 and forward[0]["status"] == "verified",
                "f2-positive: forward drat-trim ran once and verified")
        require(not runs(out, lambda c: os.path.basename(c[0]) == "dsr-trim"),
                "f2-positive: no dsr-trim stage on the witness-free route")
        require(any(os.path.basename(r["command"][0]) == "cake_lpr" and
                    r["status"] == "verified" for r in out.checker_runs),
                "f2-positive: CakeLPR verified the backward route")

        historical = os.environ.get("TACTIX_FUZZ_HISTORICAL_DSR_TRIM")
        if historical:
            require(sha256(historical) == HISTORICAL_DSR_TRIM,
                    "f2-historical (labelled diagnosis): executable is the "
                    "historical 02824056 checker")
            legacy = forward_dsr(historical, cnf, dsr,
                                 os.path.join(tmp, "f2-historical"))
            require(names_statuses(legacy) ==
                    [("dsr-trim", "verified"), ("dsr-compress", "verified"),
                     ("srcheck", "rejected")],
                    f"f2-historical (labelled diagnosis): dsr-trim and "
                    f"decode verify, Trestle rejects {names_statuses(legacy)}")

        # f2w-positive
        cnf = fixture["f2w-witnessed.cnf"]
        dsr = fixture["f2w-witnessed.dsr"]
        name, status, stdout = trestle(cnf, fixture["f2w-historical-forward.lsr"])
        require(status == "rejected" and "s INVALID" in stdout.splitlines(),
                f"f2w-positive: Trestle rejects the historical forward "
                f"elaboration (non-vacuous) [{status}]")
        out = grade(cnf, dsr, os.path.join(tmp, "f2w"))
        require(out.verdict == "UNSAT" and out.failures == [],
                f"f2w-positive: harness accepts, failures={out.failures}")
        trims = runs(out, lambda c: os.path.basename(c[0]) == "dsr-trim")
        require([("-f" in r["command"], r["status"]) for r in trims] ==
                [(False, "verified"), (True, "verified")],
                "f2w-positive: backward then forward dsr-trim both verified")
        checks = runs(out, lambda c: os.path.basename(c[0]) == "srcheck")
        require([r["status"] for r in checks] == ["verified", "verified"],
                "f2w-positive: Trestle verified both elaborations")
        require(not runs(out, lambda c: os.path.basename(c[0]) in
                         ("drat-trim", "cake_lpr")),
                "f2w-positive: no DRAT stage on the witnessed route")

        small = os.path.join(tmp, "small.cnf")
        with open(small, "w") as f:
            f.write(SMALL)
        bad = [(1, [3, 4]), (2, [3, 4]), (1, [1]), (1, [])]
        good = [(1, [1]), (1, [])]
        certs = {
            "unused-bad-ascii": b"3 4 0\nd 3 4 0\n1 0\n0\n",
            "unused-bad-binary": binary(bad),
            "unused-bad-witnessed-ascii": b"3 4 3 0\nd 3 4 0\n1 0\n0\n",
            "unused-bad-witnessed-binary": binary(
                [(1, [3, 4, 3]), (2, [3, 4]), (1, [1]), (1, [])]),
            "valid-ascii": b"1 0\n0\n",
            "valid-binary": binary(good),
        }
        for label, data in certs.items():
            cert = os.path.join(tmp, label + ".dsr")
            with open(cert, "wb") as f:
                f.write(data)
            out = grade(small, cert, os.path.join(tmp, label))
            if label.startswith("unused-bad-witnessed"):
                trims = runs(out, lambda c: os.path.basename(c[0]) ==
                             "dsr-trim")
                checks = runs(out, lambda c: os.path.basename(c[0]) ==
                              "srcheck")
                require([("-f" in r["command"], r["status"]) for r in trims]
                         == [(False, "verified"), (True, "rejected")] and
                        any(line.startswith("Error:") for line in
                            (trims[1]["stdout"] + "\n" +
                             trims[1]["stderr"]).splitlines()),
                        f"{label}: backward dsr-trim verifies, forward "
                        f"dsr-trim rejects with an Error line "
                        f"{[(r['command'][1], r['status']) for r in trims]}")
                require([r["status"] for r in checks] == ["verified"],
                        f"{label}: Trestle verified the backward elaboration "
                        f"(the forward audit is the catching oracle)")
                require(not runs(out, lambda c: os.path.basename(c[0]) in
                                 ("drat-trim", "cake_lpr")),
                        f"{label}: no DRAT stage on the witnessed route")
                require(out.failures == [FORWARD_DSR_REASON],
                        f"{label}: failure is exactly the forward dsr-trim "
                        f"audit, got {out.failures}")
                continue
            forward = runs(out, is_forward_drat)
            backward = runs(out, is_backward_drat)
            cake = runs(out, lambda c: os.path.basename(c[0]) == "cake_lpr")
            require(len(forward) == 1 and backward and cake,
                    f"{label}: forward, backward and CakeLPR stages all ran")
            if label.startswith("unused-bad"):
                require(forward and forward[0]["status"] == "rejected" and
                        "s NOT VERIFIED" in forward[0]["stdout"].splitlines(),
                        f"{label}: forward drat-trim rejects the unused lemma "
                        f"(s NOT VERIFIED)")
                require(backward and backward[0]["status"] == "verified" and
                        cake and cake[0]["status"] == "verified",
                        f"{label}: backward drat-trim/CakeLPR accept it "
                        f"(the forward audit is the catching oracle)")
                require(out.failures == [FORWARD_REASON],
                        f"{label}: failure is exactly the forward audit, "
                        f"got {out.failures}")
            else:
                require(out.failures == [] and forward and
                        forward[0]["status"] == "verified",
                        f"{label}: accepted, failures={out.failures}")
    print("EVERY-LEMMA CONTROLS " + ("OK" if not problems else
                                     f"FAILED ({len(problems)})"))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
