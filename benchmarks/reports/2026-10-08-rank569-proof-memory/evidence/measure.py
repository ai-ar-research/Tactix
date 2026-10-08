"""Peak-RSS measurement of DSR checkers on one proof (r569 checker-memory study).

Checker/reference measurement is the stated bounded exception to the
runtime-task rule: only JSON statistics, memrep lines, and hashes persist;
CNF copies and LSR outputs live in node-local scratch and are deleted here.
"""
import argparse, hashlib, json, lzma, os, resource, shutil, signal, subprocess, sys, time
from pathlib import Path

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 22), b''): h.update(b)
    return h.hexdigest()

def tail(p, n=4000):
    try:
        with open(p, 'rb') as f:
            f.seek(0, 2); s = f.tell(); f.seek(max(0, s - n)); return f.read().decode('utf8', 'replace')
    except OSError: return ''

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cnf', required=True); ap.add_argument('--proof', required=True)
    ap.add_argument('--out', required=True); ap.add_argument('--scratch', required=True)
    ap.add_argument('--checker', action='append', default=[], help='name=path')
    ap.add_argument('--timeout', type=int, default=30000)
    ap.add_argument('--stats', required=True)
    a = ap.parse_args()
    scratch = Path(a.scratch); scratch.mkdir(parents=True, exist_ok=False)
    res = dict(schema='r569-checker-memory-v1', node=os.uname().nodename, job=os.environ.get('SLURM_JOB_ID'),
               cnf=a.cnf, proof=a.proof, proof_bytes=os.path.getsize(a.proof), checkers={})
    cnf = scratch / 'input.cnf'
    if a.cnf.endswith('.xz'):
        with lzma.open(a.cnf) as src, open(cnf, 'wb') as dst: shutil.copyfileobj(src, dst, 1 << 22)
    else: shutil.copyfile(a.cnf, cnf)
    res['cnf_sha256'] = sha(cnf)
    t = time.time()
    ps = subprocess.run([a.stats, a.proof], capture_output=True, text=True)
    res['proofstats'] = dict(exit=ps.returncode, out=ps.stdout.strip(), err=ps.stderr[-2000:], seconds=round(time.time() - t, 1))
    procs = {}
    for spec in a.checker:
        name, path = spec.split('=', 1)
        lsr = scratch / f'{name}.lsr'; so = scratch / f'{name}.stdout'; se = scratch / f'{name}.stderr'
        digest = sha(path)
        p = subprocess.Popen([path, str(cnf), a.proof, str(lsr)], stdout=open(so, 'wb'), stderr=open(se, 'wb'))
        procs[p.pid] = (name, path, p, lsr, so, se, time.time(), digest)
    deadline = time.time() + a.timeout
    while procs:
        pid, status, ru = os.wait4(-1, os.WNOHANG)
        if pid == 0:
            if time.time() > deadline:
                for v in procs.values(): v[2].send_signal(signal.SIGKILL)
                deadline = float('inf')
            time.sleep(2); continue
        name, path, p, lsr, so, se, t0, digest = procs.pop(pid)
        code = os.waitstatus_to_exitcode(status) if hasattr(os, 'waitstatus_to_exitcode') else status
        err = open(se, 'rb').read().decode('utf8', 'replace')
        memrep = [l for l in err.splitlines() if l.startswith('c memrep')]
        r = dict(path=path, binary_sha256=digest, exit=code, wall_seconds=round(time.time() - t0, 1),
                 peak_rss_kb=ru.ru_maxrss, memrep=memrep,
                 stdout_tail=tail(so), stderr_tail='\n'.join(l for l in err.splitlines() if not l.startswith('c memrep'))[-3000:])
        if lsr.exists():
            r['lsr_bytes'] = lsr.stat().st_size; r['lsr_sha256'] = sha(lsr); lsr.unlink()
        so.unlink(); se.unlink()
        res['checkers'][name] = r
        Path(a.out).write_text(json.dumps(res, indent=1))
    Path(a.out).write_text(json.dumps(res, indent=1))
    cnf.unlink(); shutil.rmtree(scratch)

if __name__ == '__main__': main()
