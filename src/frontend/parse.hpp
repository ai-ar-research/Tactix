// Streaming DIMACS ingestion (plan §6): tolerant of comments anywhere,
// arbitrary whitespace, missing final newline/zero; grows past a lying
// header. Shared clause finalization (dedupe, tautology skip, unit/empty
// handling) used by both the file parser and the IPASIR-style C API.
#pragma once

#include "kernel/kernel.hpp"

#include <cstdio>
#include <vector>

namespace tactix {

// returns nullptr on success, a static error string otherwise
const char *parse_dimacs(FILE *, Kernel &);

// finalize one clause given as internal literals (scratch is consumed):
// dedupes, drops tautologies, assigns units, records empty clauses
void ingest_clause(Kernel &, std::vector<Lit> &scratch);

} // namespace tactix
