// Streaming DIMACS ingestion (plan §6): comments anywhere, arbitrary
// whitespace, a missing final newline or zero, growth past the header. Clause
// finalization is shared with the C API. A missing final zero is accepted as
// input only; no proof may rely on it (cake_lpr refuses such files).
#pragma once

#include "kernel/kernel.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

// Returns nullptr after calling finish_input, or a static error string without
// calling it.
const char *parse_dimacs(FILE *, Kernel &);
// The same parser with an explicit block size (tests use small blocks).
const char *parse_dimacs_with_block(FILE *, Kernel &, size_t block_bytes);

// A DIMACS header's effect on the kernel: the declared variables exist, and the
// counts decide which input shapes are recorded.
void declare_input(Kernel &, uint32_t variables, uint64_t clauses);

// Finalize one clause given as internal literals (scratch is consumed):
// dedupes, drops tautologies, assigns units, records empty clauses.
void ingest_clause(Kernel &, std::vector<Lit> &scratch);

// End of input: retire variables absent from the input and emit any refutation
// found; once, after the last ingest_clause (parse_dimacs, or api.cpp).
void finish_input(Kernel &);

// Ingest zero-delimited DIMACS literals (Kernel::startup_input), then finish_input.
void ingest_dimacs(Kernel &, const std::vector<int32_t> &literals);

// Attach a proof after eager import by replaying the original clauses into a
// temporary proof kernel; solver state stays untouched.
void attach_replayed_input_proof(Kernel &, Proof &);

} // namespace tactix
