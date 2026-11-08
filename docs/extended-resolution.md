# Extended resolution and the PHP strategy

The first milestone is a standalone PR providing exact Boolean definitions
through the public strategy interface, with a dedicated strategy for generated
pigeonhole problems. Automatic nonlinear-expression discovery and mixed
Boolean/XOR performance experiments follow in separate work.

## Public operation

`tactix_define_extension(s, gate, left, right)` introduces a fresh variable
equivalent to the AND, OR or XOR of two signed permanent literals. It returns
the positive external identity, or `-1` on an inapplicable request. Operands
can include outputs of earlier calls. Definitions require a propagated,
consistent sparse root and live operand identities. Root-assigned operands,
repeated operands and complementary operands are supported.

The operation is the exact public instance surface of the registered
`ExtendedResolution` rule. Its canonical application is a no-op. DefaultPolicy
does not schedule it. Definitions emit ordered RAT additions; no new proof
format is needed. Each old assignment has exactly one extension, and output
models omit the fresh variables.

The output is exposed to `SemanticVariableAdvice` as an introduced theory
variable. A strategy can choose its priority dose or branch on an eligible
output directly. Defining a variable does not itself apply a score bump.
Advice is state-bound and should be consumed before another action stales it.

## Variable lifetime

New summaries must survive long enough to participate in reasoning and search.
Extension status alone does not protect a variable from BVE or substitution.
The PHP strategy therefore schedules only definitions, checked consequences,
and root propagation. It runs no elimination, substitution, sweep, or default
preprocessing, and reports elimination counters. Root propagation may fix a
summary; that is useful reasoning, not premature elimination.

For later mixed-formula experiments, the strategy must similarly retain
definitions through an initial search period, then explicitly decide when to
allow simplification. A selective freeze/release interface for handing these
variables to arbitrary DefaultPolicy preprocessing remains separate work;
this PR does not claim a global variable-freezing guarantee.

## Generator and dedicated solver

The repository already contains the standard pairwise PHP generator in
`examples/cnf/generate.py`. Its explicit CLI generates ordinary DIMACS:

```sh
python3 examples/cnf/generate.py --pigeonhole 21 20 --output php-21-20.cnf
build/release/examples/tactix-php-er php-21-20.cnf php-21-20.pbp
```

The dedicated solver accepts the generator's canonical row-major encoding of
PHP(n+1,n), for n >= 2. It validates every clause and literal before invoking
the strategy. Other encodings, reordered inputs, malformed files, and SAT
PHP(n,n) controls are rejected as unsupported. It does not invoke the existing
DirectPhpRefutation rule or general CDCL search.

## Polynomial construction

At a level with h holes and h+1 pigeons, let `p[i,j]` denote the current
assignment variables. For i < h and j < h-1 introduce

```
t[i,j] = p[i,h-1] AND p[h,j]
q[i,j] = p[i,j] OR t[i,j]
```

These variables relocate an occupant of the final hole to the hole used by
the final pigeon. Derive the h pigeon clauses over q and the collision clauses
for h-1 holes. For each collision pair, derive its two ternary versions guarded
by opposite signs of `p[h,j]`, then derive their binary resolvent. Each clause
is RUP from the preceding constraints and definitions. Pigeon clauses are RUP
by the old pigeon clauses and the collision in the final hole.

The implementation uses the existing public explicit SR validator for these
RUP consequences. Repeat with q until root propagation refutes PHP(2,1).
There are at most `2*n*(n+1)*(n-1)/3` fresh variables and O(n^4) consequence
requests, with O(n^4) total clause literals. Validation uses propagation,
not branching, so the complete algorithm is polynomial. The general SR
validator currently reconstructs a validation formula for each application;
that can make its practical scaling much worse than the construction size.

## Validation and scaling plan

- Exhaustively check binary definition semantics for signed, repeated and
  complementary operands, plus composition, namespace holes, invalid-request
  atomicity, root propagation and public score/branch control.
- Test generated PHP instances and unsupported-input controls through the
  dedicated executable. Require zero search decisions and zero elimination.
- Compare release builds of the dedicated strategy, default Tactix, and pinned
  Kissat on identical generated inputs. Use matched CPU affinity, rotate run
  order, retain every timeout, and charge parsing and validation in wall time.
- Report runtime against both hole count and input size, alongside definition
  and consequence counts. Standard pairwise PHP has Theta(n^3) clauses, so
  runtime linear in hole count is impossible for an explicit-input solver.
  Near-linear scaling in input size is the optimization target; the current
  O(n^4) construction does not establish it.
- Publish Andrew's report with measured plots and explicit limitations. Do not
  infer a speedup over full Tactix from resolution lower bounds: full Tactix
  already includes specialized PHP reasoning.

## Subsequent mixed-formula experiment

Automatically choose shared nonlinear expressions in bounded gate
neighborhoods. Compare baseline, definitions, definitions plus priority, and
matched priority changes on existing variables. Charge discovery, use at least
ten real training instances across two families, and require at least a 25%
per-family time reduction without a lost baseline solve. Shared measurements
from the ETH paper motivate the selection objective; adding definitions alone
does not implement its deformation/interpolation algorithm or inherit its bound.
