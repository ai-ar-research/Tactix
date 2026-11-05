// Streaming DIMACS ingestion (plan §6): tolerant of comments anywhere,
// arbitrary whitespace and a missing final newline or zero; grows past a lying
// header. Shared clause finalization (dedupe, tautology skip, unit/empty
// handling) used by both the file parser and the IPASIR-style C API.
//
// A missing final newline is valid DIMACS. A missing final zero is not: no
// checker accepts such a file (cake_lpr refuses it), so a proof citing its id cannot verify. Tolerating it is an
// input-validation choice, never something an emitted proof may rely on. See
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2.1.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

// Returns nullptr on success, a static error string otherwise. Calls
// finish_input before returning.
const char *parse_dimacs(FILE *, Kernel &);
// The same parser with an explicit input block size. Production uses the
// 1 MiB block above; smaller blocks exist so tests can put every token and
// line boundary on a block boundary.
const char *parse_dimacs_with_block(FILE *, Kernel &, size_t block_bytes);

// Finalize one clause given as internal literals (scratch is consumed):
// dedupes, drops tautologies, assigns units, records empty clauses.
void ingest_clause(Kernel &, std::vector<Lit> &scratch);

// End of input. Retires variables absent from the input and emits any refutation
// found during it. Runs exactly once, after the last ingest_clause and before any
// rule: parse_dimacs does it for the file path, api.cpp for the C API.
void finish_input(Kernel &);

// Attach a proof after eager import by replaying the exact original clauses
// into a temporary proof kernel. The existing solver's assignments, watches,
// reasons and policy state stay untouched.
void attach_replayed_input_proof(Kernel &, Proof &);

} // namespace tactix
