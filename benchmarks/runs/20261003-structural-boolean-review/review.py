"""Static original-CNF inspection and fixed truth-table controls; no solver run."""
from pathlib import Path
import collections, hashlib, itertools, json, lzma
ROOT = Path(__file__).resolve().parents[3]
p = ROOT/'benchmarks/cache/tierD24-26/d8225a58e2ca7c2fb93c533f406c02d5-abw-K-dwt__234.mtx-w50.cnf.xz'
raw = lzma.open(p, 'rb').read()
clauses = []; pending = []
for line in raw.decode().splitlines():
    if not line or line[0] in 'cp': continue
    for token in line.split():
        x = int(token)
        if x: pending.append(x)
        else: clauses.append(pending); pending = []
assert not pending and len(clauses) == 178213
ids = [59,60,61,62]
assert [clauses[i-1] for i in ids] == [[-49,-50],[49,50,13739],[49,-50,-13739],[-49,-13739]]
def sat(cs, assignment):
    return all(any(assignment[abs(x)] == (x>0) for x in c) for c in cs)
models = []
for values in itertools.product([False,True],repeat=3):
    a = dict(zip([49,50,13739], values))
    if sat([clauses[i-1] for i in ids], a):
        assert a[13739] == (not (a[49] or a[50]))
        assert not a[50] or not a[13739]
        models.append(a)
# Negate proposed clause: 50 and 13739 true. Parent 59 forces -49;
# parent 61 conflicts. This is a two-parent RUP certificate, not heuristic inference.
a = {50:True,13739:True,49:False}
assert not sat([clauses[60]], a)
assert sat([clauses[58]], a)
# Exact unsigned, unshortened four-by-four recognition fixture from its test.
n=4
fixture = [[r*n+c+1 for r in range(n)] for c in range(n)]
fixture += [[-(a*n+c+1),-(b*n+c+1)] for c in [2,3] for a,b in [(0,1),(1,2)]]
model = {v: v>12 for v in range(1,17)}
assert sat(fixture, model)
row_counts = [sum(model[r*n+c+1] for c in range(n)) for r in range(n)]
assert row_counts == [0,0,0,4]  # Not a permutation: recognizer is not a theorem.
result = dict(scope='Static scan and fixed microcontrols; no target solver/proof run or performance claim',
 input=str(p.relative_to(ROOT)), input_sha256=hashlib.sha256(raw).hexdigest(),
 clause_widths=dict(collections.Counter(map(len,clauses))),
 certified_fragment=dict(source_clause_occurrences=ids, clauses=[clauses[i-1] for i in ids],
 models=models, meaning='13739 iff neither 49 nor 50; fragment also forbids selecting both',
 proposed_constraint=[-50,-13739], rup_parent_order=[59,61],
 already_explicit=any(set(c)=={-50,-13739} for c in clauses),
 limitation='Only this fragment is certified here; no claim that all rows are permutations or all interval gates match'),
 heuristic_countermodel=dict(clauses=fixture, model=model, row_counts=row_counts,
 consequence='Heuristic rectangle/edge recognition does not certify per-row exactly-one'))
Path(__file__).with_name('result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
