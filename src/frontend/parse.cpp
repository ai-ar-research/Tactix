#include "frontend/parse.hpp"

#include "policies/heuristics.hpp"
#include "proof/proof.hpp"

#include <algorithm>
#include <array>

namespace tactix {

// N2 and the input path. A checker numbers the original file's clauses 1..N by
// position — lrat-check and cake_lpr both ignore the header count — so a chain
// citing id `i` asks it to propagate the clause the file holds at position `i`,
// and every normalization ingest_clause performs is a potential divergence.
// The normalization cases and verdicts live in
// benchmarks/reports/2026-08-05-input-clause-normalization-n2.md §2. Repeats
// and root-false literals narrow the stored clause, so LRAT restates it under a
// fresh id with root-unit hints. DRAT has no clause identities: deduplication is
// equivalent, root shrinking is RUP under the derived units, and unmatched
// deletions are hints. A mid-input refutation remains deferred to finish_input
// so LRAT's original id namespace is never crossed.
static void restate_input(Kernel &k, uint64_t id, const std::vector<Lit> &lits,
                          const std::vector<Lit> &units, Ref ref) {
  // Record that the checker's clause `id` is not the clause just stored. The
  // count is unconditional, being the reachability probe for this path; the
  // restatement matters only with a proof, since without one no id names
  // anything.
  k.stats.input_restated++;
  if (!k.proof || !k.proof->lrat())
    return;
  Kernel::InputRestatement r;
  r.id = id;
  r.begin = static_cast<uint32_t>(k.input_restate_lits.size());
  r.size = static_cast<uint32_t>(lits.size());
  r.unit_begin = static_cast<uint32_t>(k.input_restate_units.size());
  r.unit_size = static_cast<uint32_t>(units.size());
  r.ref = ref;
  k.input_restate_lits.insert(k.input_restate_lits.end(), lits.begin(),
                              lits.end());
  k.input_restate_units.insert(k.input_restate_units.end(), units.begin(),
                               units.end());
  k.input_restatements.push_back(r);
}

// Kissat closes every new input unit immediately, so later clauses are added
// against the resulting root assignment.  Do the same for the proof-less and
// DRAT paths.  Native LRAT must defer this closure until all original input
// ids have been consumed: propagation-derived units need fresh ids, and a
// derived id may not precede a later original id in the LRAT namespace.
static void propagate_input_unit(Kernel &k) {
  if (k.proof && k.proof->lrat())
    return;
  const bool was_inprocessing = k.inprocessing;
  k.inprocessing = true; // input scans do not spend the search-tick budget
  const bool conflict = kernel_propagate(k);
  k.inprocessing = was_inprocessing;
  if (!conflict)
    return;
  if (k.proof)
    k.proof->add_empty(0, nullptr, 0); // DRAT needs no antecedent ids
  k.inconsistent = true;
}

void ingest_clause(Kernel &k, std::vector<Lit> &scratch) {
  // Every input clause consumes one proof id in input order, including
  // tautologies and duplicates that are never stored (LRAT input numbering).
  const uint64_t id = ++k.next_clause_id;

  // Grow the kernel to cover every mentioned variable.
  Var maxv = 0;
  for (const Lit l : scratch)
    maxv = std::max(maxv, var_of(l));
  if (!scratch.empty())
    kernel_reserve_vars(k, maxv + 1);

  // Kissat's default `tumble=1` imports variables by first occurrence and
  // activates the literals of each surviving clause in that same file order
  // (internal.c `kissat_add` -> flags.c `kissat_activate_literals`).  Retain
  // that order across the set-semantics check below.  Do not publish it yet: a
  // tautology activates nothing and returns below.
  const size_t file_size = scratch.size();
  std::array<Lit, 2> small_order{};
  std::vector<Lit> large_order;
  const Lit *file_order;
  if (file_size <= small_order.size()) {
    std::copy(scratch.begin(), scratch.end(), small_order.begin());
    file_order = small_order.data();
  } else {
    large_order = scratch;
    file_order = large_order.data();
  }
  for (size_t i = 0; i < file_size; i++)
    kernel_record_import(k, var_of(file_order[i]));

  // Set semantics: dedupe; tautologies contribute nothing.  The sorted copy
  // is only the membership check.  Original clauses are stored in stable
  // first-occurrence file order, as Kissat does while all literals are
  // unassigned; sorting storage here changes the initial watched pair and the
  // order of every affected watch list.
  if (file_size == 2) {
    if (scratch[0] == negate(scratch[1]))
      return;
    if (scratch[0] == scratch[1])
      scratch.resize(1);
  } else if (file_size > 2) {
    std::sort(scratch.begin(), scratch.end());
    scratch.erase(std::unique(scratch.begin(), scratch.end()), scratch.end());
    for (size_t i = 1; i < scratch.size(); i++)
      if (scratch[i] == negate(scratch[i - 1]))
        return; // tautology
    const std::vector<Lit> canonical = scratch;
    std::vector<uint8_t> emitted(canonical.size(), 0);
    scratch.clear();
    scratch.reserve(canonical.size());
    for (size_t i = 0; i < file_size; i++) {
      const Lit lit = file_order[i];
      const auto it = std::lower_bound(canonical.begin(), canonical.end(), lit);
      assert(it != canonical.end() && *it == lit);
      const size_t pos = static_cast<size_t>(it - canonical.begin());
      if (!emitted[pos]) {
        emitted[pos] = 1;
        scratch.push_back(lit);
      }
    }
  }
  // Kissat internal.c:kissat_add normalizes every input clause against units
  // already fixed while parsing.  A root-true literal satisfies the clause;
  // a root-false literal is removed, with its true complement retained as the
  // LRAT antecedent for the deferred restatement below.  The normal and DRAT
  // paths propagate new units immediately, matching Kissat; native LRAT alone
  // defers derived units because their ids cannot precede later input ids.
  const bool native_lrat = k.proof && k.proof->lrat();
  std::vector<Lit> root_units;
  auto out = scratch.begin();
  for (const Lit lit : scratch) {
    const int8_t value = k.values[lit];
    if (value > 0)
      return; // satisfied input clause: consume its id, activate/store nothing
    if (value < 0) {
      if (native_lrat)
        root_units.push_back(negate(lit));
    }
    else
      *out++ = lit;
  }
  scratch.erase(out, scratch.end());

  // N2: the checker holds the file clause under `id`, repeats and all, so a
  // shrunk clause may not keep it. Restate at finish_input, but only when this
  // clause is actually stored — a dropped clause's id is never cited.
  const bool shrunk = scratch.size() < file_size;

  if (k.inconsistent)
    return; // already refuted; further clauses don't matter

  // Activation, kissat's `tumble` order (heuristics_seed_activation_order).
  // Placed after the tautology return and before the store so it matches
  // kissat's per-clause activation in `kissat_add`: a tautology activates
  // nothing, and first activation follows file-literal order rather than the
  // canonical sorted storage order.  input_activated removes repeated
  // literals in one clause and variables activated by an earlier clause.
  if (k.tumble) {
    if (k.input_activated.size() < k.nvars)
      k.input_activated.resize(k.nvars, 0);
    for (size_t i = 0; i < file_size; i++) {
      const Var v = var_of(file_order[i]);
      if (k.values[make_lit(v, false)] == 0 && !k.input_activated[v]) {
        k.input_activated[v] = 1;
        k.input_activation_order.push_back(v);
      }
    }
  }

  if (scratch.empty()) { // empty input clause: immediate refutation
    k.inconsistent = true;
    k.input_refuted = true; // the input empty clause is the whole chain
    k.input_refutation_id = id;
    k.input_refutation_units = root_units;
  } else if (scratch.size() == 1) {
    const Lit u = scratch[0];
    if (k.values[u] > 0)
      return; // duplicate unit (its id is consumed, never referenced)
    if (k.values[u] < 0) { // contradicting unit: refuted at the root
      k.inconsistent = true;
      k.input_refuted = true;
      k.input_refutation_id = id;
      k.input_refutation_units = {negate(u)};
      return;
    }
    k.unit_ids[var_of(u)] = id;
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0);
    kernel_assign_unit(k, u);
    propagate_input_unit(k);
  } else if (scratch.size() == 2) {
    // Duplicate binary: id consumed, never stored, following the duplicate-unit
    // precedent — the checker keeps its never-referenced input copy and no
    // proof line is emitted. Nothing is deleted during parse, so presence in
    // binary_ids is equivalent to a live stored copy. Kissat's dense mode
    // likewise dedups before inprocessing runs; duplicated pairs otherwise
    // inflate Factor's watch-score seeds and block BVE through the
    // singular-delete hazard, and on an input carrying six figures of them this
    // deduplication alone is worth an order of magnitude.
    if (k.binary_ids.count(kernel_binary_key(scratch[0], scratch[1])))
      return;
    kernel_add_binary_clause(k, scratch[0], scratch[1], id);
    if (shrunk)
      restate_input(k, id, scratch, root_units, 0);
  } else {
    const Ref ref = kernel_add_large_clause(
        k, scratch.data(), static_cast<uint32_t>(scratch.size()), false, 0, id);
    if (shrunk)
      restate_input(k, id, scratch, root_units, ref);
  }
}

void finish_input(Kernel &k) {
  // Search never has more than one assignment per variable, while its level
  // stack cannot exceed the same population.  Reserve both once, after the
  // DIMACS header/import stream has fixed the population, so the propagation
  // hot path does not carry vector growth and relocation into assignment.
  k.trail.reserve(k.nvars);
  k.frames.reserve(static_cast<size_t>(k.nvars) + 1);
  // Input ids are now settled: next_clause_id equals the number of input
  // clauses, exactly the checker's numbering, because ingest_clause consumes
  // one id per clause and nothing else consumed any. Derived ids start here.
  // Restatements are non-empty only when a proof was attached before the input;
  // attaching one later cannot be repaired, the input ids are already spent.
  assert(k.input_restatements.empty() || k.proof);
  // A declared but unmentioned variable is absent from Kissat's import map.
  // Tactix has already reserved it from the DIMACS header; rank it after every
  // actually imported variable so it cannot perturb reference-ordered ties.
  for (Var v = 0; v < k.nvars; v++)
    kernel_record_import(k, v);
  std::vector<uint64_t> chain;
  for (const Kernel::InputRestatement &r : k.input_restatements) {
    const Lit *lits = k.input_restate_lits.data() + r.begin;
    const uint64_t nid = ++k.next_clause_id;
    chain.clear();
    if (k.proof->lrat()) {
      for (uint32_t i = 0; i < r.unit_size; i++) {
        const Lit unit = k.input_restate_units[r.unit_begin + i];
        assert(k.values[unit] > 0);
        const uint64_t uid = k.unit_ids[var_of(unit)];
        assert(uid);
        chain.push_back(uid);
      }
      // Root units first remove every root-false literal; the original file
      // clause then conflicts under the negated normalized clause. Duplicate-
      // only normalization has no units and retains the old one-hint proof.
      chain.push_back(r.id);
    }
    k.proof->add_rup(nid, lits, r.size,
                     chain.empty() ? nullptr : chain.data(),
                     static_cast<uint32_t>(chain.size()));
    if (r.size == 1) {
      assert(k.unit_ids[var_of(lits[0])] == r.id);
      k.unit_ids[var_of(lits[0])] = nid;
    } else if (r.size == 2) {
      const uint64_t key = kernel_binary_key(lits[0], lits[1]);
      assert(k.binary_ids.at(key) == r.id);
      k.binary_ids.insert_or_assign(key, nid);
    } else {
      Clause *c = clause_at(k.arena, r.ref);
      assert(c->id() == r.id);
      kernel_inherit_clause_order(k, nid, r.id);
      c->set_id(nid);
    }
  }
  k.input_restatements.clear();
  k.input_restate_lits.clear();
  k.input_restate_units.clear();
  // The decision order is `H` state.  If `H` is already attached, publish the
  // imported-variable permutation now.  Otherwise retain it until
  // ScopedHeuristics attaches at solve entry: Kissat's random brancher samples
  // `VARS` (the imported population), not every variable admitted by the DIMACS
  // header, so dropping this order also changes the RNG modulus.
  heuristics_seed_activation_order(k, k.input_activation_order);
  if (k.h) {
    k.input_activation_order.clear();
    k.input_activation_order.shrink_to_fit();
  }
  k.input_activated.clear();
  k.input_activated.shrink_to_fit();
  if (k.input_refuted) {
    k.input_refuted = false;
    if (k.proof) {
      chain.clear();
      if (k.proof->lrat()) {
        for (const Lit unit : k.input_refutation_units) {
          assert(k.values[unit] > 0);
          const uint64_t uid = k.unit_ids[var_of(unit)];
          assert(uid);
          chain.push_back(uid);
        }
        chain.push_back(k.input_refutation_id);
      }
      k.proof->add_empty(++k.next_clause_id,
                         chain.empty() ? nullptr : chain.data(),
                         static_cast<uint32_t>(chain.size()));
    }
    k.input_refutation_units.clear();
  }
}

const char *parse_dimacs(FILE *f, Kernel &k) {
  std::vector<Lit> scratch;
  // Parsing is single-threaded and owns `f`. Pull a block at a time rather than
  // entering stdio for every byte; a three-gigabyte binary CNF otherwise pays
  // tens of millions of calls even with the stream lock disabled.
  struct Reader {
    FILE *file;
    std::array<unsigned char, 1 << 20> buffer{};
    size_t pos = 0, end = 0;
    int next() {
      if (pos == end) {
        end = fread(buffer.data(), 1, buffer.size(), file);
        pos = 0;
        if (!end)
          return EOF;
      }
      return buffer[pos++];
    }
  } reader{f};
  int c = reader.next();
  for (;;) {
    // Skip whitespace.
    while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      c = reader.next();
    if (c == EOF)
      break;
    if (c == 'c') { // comment line
      while (c != '\n' && c != EOF)
        c = reader.next();
      continue;
    }
    if (c == 'p') { // header: "p cnf <vars> <clauses>" (counts advisory)
      while (c != '\n' && c != EOF) {
        if (c >= '0' && c <= '9') { // first number = declared variables
          uint64_t n = 0;
          while (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            if (n > (1u << 28))
              return "variable count out of range";
            c = reader.next();
          }
          kernel_reserve_vars(k, static_cast<uint32_t>(n));
          while (c == ' ' || c == '\t' || c == '\r')
            c = reader.next();
          uint64_t clauses = 0;
          while (c >= '0' && c <= '9') {
            clauses = clauses * 10 + (c - '0');
            c = reader.next();
          }
          k.original_input_clauses = clauses;
          // Avoid repeated multi-gigabyte rehashes on binary-dominated inputs.
          // Binary encodings commonly contain almost nothing else.  Reserve up
          // to the declared clause count, capped by three times the variable
          // population so a huge long-clause input does not reserve a giant
          // binary table it will never use.
          const uint64_t predicted_binaries =
              std::min<uint64_t>(clauses, 3 * n);
          k.binary_ids.reserve(static_cast<size_t>(predicted_binaries));
          while (c != '\n' && c != EOF)
            c = reader.next();
          break;
        }
        c = reader.next();
      }
      continue;
    }
    // a literal
    bool neg = false;
    if (c == '-') {
      neg = true;
      c = reader.next();
    }
    if (c < '0' || c > '9')
      return "expected literal";
    uint64_t n = 0;
    while (c >= '0' && c <= '9') {
      n = n * 10 + (c - '0');
      if (n > (1u << 28))
        return "literal out of range";
      c = reader.next();
    }
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n' && c != EOF)
      return "unexpected character after literal";
    if (n == 0) {
      ingest_clause(k, scratch);
      scratch.clear();
    } else {
      if (neg)
        scratch.push_back(internalize(-static_cast<int>(n)));
      else
        scratch.push_back(internalize(static_cast<int>(n)));
    }
  }
  if (!scratch.empty()) // tolerate a missing final 0
    ingest_clause(k, scratch);
  finish_input(k);
  return nullptr;
}

} // namespace tactix
