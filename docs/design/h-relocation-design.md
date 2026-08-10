# Heuristic-state relocation — frozen compact record

**Status:** landed architecture record.

## 1. Durable boundary

`Kernel` may hold an opaque, non-owning pointer to heuristic state `H`, but
kernel correctness cannot depend on `H` being present. DefaultPolicy owns and
attaches the concrete heuristic state. A null-`H` solver remains sound,
deterministic, and capable of unguided decisions; only search quality degrades.

Proof rules may read kernel facts and policy-scoped neutral interfaces, not
concrete policy classes. Guidance-only operations mutate `H` and emit no proof
delta. Model, proof, reconstruction, and external variable identities remain
kernel-owned.

## 2. Landed validation

The relocation covered VMTF/EVSIDS state, saved/target phases, mode state, and
decision selection. Focused tests exercise null-`H`, DefaultPolicy attachment,
and the kernel/header boundary. Current architecture is owned by
`DEVELOPMENT_PLAN.md` §5 and the compact H-only decision in
`docs/implementation-notes.md`.

## 3. Retired plan material

The original Track-H checklist, per-file migration inventory, and intermediate
hybrid states are complete. They are not live implementation instructions.
