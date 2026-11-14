// The EventTrace events whose free `event_trace_<name>` function only
// forwards to the active trace, as TACTIX_TRACE_FORWARD(name, (parameters),
// (arguments)).  event_trace.hpp expands the list into the member and free
// declarations (production builds: inline no-ops) and event_trace.cpp into the
// forwarders; each includer defines TACTIX_TRACE_FORWARD, and this file
// undefines it.
TACTIX_TRACE_FORWARD(watch_scan, (const Kernel &k, Lit propagated),
                     (k, propagated))
TACTIX_TRACE_FORWARD(watch_move, (const Kernel &k, Lit propagated, Lit target,
                     Lit blocking, const Clause *clause),
                     (k, propagated, target, blocking, clause))
TACTIX_TRACE_FORWARD(watch_cost, (const Kernel &k, Lit propagated,
                     uint64_t words_before, uint64_t words_after, uint64_t base,
                     uint64_t binary_assignments, uint64_t large_dereferences,
                     uint64_t relocations, uint64_t large_assignments,
                     uint64_t total), (k, propagated, words_before, words_after,
                     base, binary_assignments, large_dereferences, relocations,
                     large_assignments, total))
TACTIX_TRACE_FORWARD(learned_stage, (const Kernel &k, const char *event,
                     const std::vector<Lit> &clause), (k, event, clause))
TACTIX_TRACE_FORWARD(clause_glue, (const Kernel &k, const char *source,
                     const Clause *clause, uint32_t old_glue,
                     uint32_t new_glue),
                     (k, source, clause, old_glue, new_glue))
TACTIX_TRACE_FORWARD(clause_used, (const Kernel &k, const Clause *clause,
                     uint32_t mode), (k, clause, mode))
TACTIX_TRACE_FORWARD(retier, (const Kernel &k, const RetierSchedule &tiers,
                     uint32_t mode, const uint64_t *used_glue),
                     (k, tiers, mode, used_glue))
TACTIX_TRACE_FORWARD(phase_pick, (const Kernel &k, Var var, int8_t target,
                     int8_t saved, int8_t result),
                     (k, var, target, saved, result))
TACTIX_TRACE_FORWARD(target_save, (const Kernel &k), (k))
TACTIX_TRACE_FORWARD(phase_save, (const Kernel &k, Lit lit), (k, lit))
TACTIX_TRACE_FORWARD(sweep_schedule, (const Kernel &k, Var seed, uint64_t pos,
                     uint64_t neg, uint64_t rank, uint64_t import_order),
                     (k, seed, pos, neg, rank, import_order))
TACTIX_TRACE_FORWARD(sweep_seed, (const Kernel &k, Var seed), (k, seed))
TACTIX_TRACE_FORWARD(sweep_environment, (const Kernel &k, Var seed,
                     uint32_t vars, uint32_t clauses, uint64_t hash),
                     (k, seed, vars, clauses, hash))
TACTIX_TRACE_FORWARD(sweep_environment_clause, (const Kernel &k, Var seed,
                     uint32_t ordinal, const Lit *lits, uint32_t size),
                     (k, seed, ordinal, lits, size))
TACTIX_TRACE_FORWARD(sweep_query, (const Kernel &k, Var seed, uint32_t ordinal,
                     const char *kind, Lit a, Lit b, int result, uint64_t ticks,
                     uint64_t conflicts),
                     (k, seed, ordinal, kind, a, b, result, ticks, conflicts))
TACTIX_TRACE_FORWARD(sweep_model, (const Kernel &k, Var seed, uint32_t ordinal,
                     const Var *vars, const int8_t *values, uint32_t size),
                     (k, seed, ordinal, vars, values, size))
TACTIX_TRACE_FORWARD(sweep_flip, (const Kernel &k, Var seed, const char *stage,
                     Lit lit, bool result, uint64_t ticks),
                     (k, seed, stage, lit, result, ticks))
TACTIX_TRACE_FORWARD(sweep_effort, (const Kernel &k, uint32_t external,
                     uint64_t ticks_before, uint64_t ticks_after,
                     uint64_t solves),
                     (k, external, ticks_before, ticks_after, solves))
TACTIX_TRACE_FORWARD(sweep_environment_var, (const Kernel &k, Var seed,
                     uint32_t ordinal, Var var, uint32_t depth),
                     (k, seed, ordinal, var, depth))
TACTIX_TRACE_FORWARD(sweep_binary_attempt, (const Kernel &k, Var seed,
                     uint32_t ordinal, Lit lit, Lit other, int32_t lit_depth,
                     int32_t other_depth, const char *disposition), (k, seed,
                     ordinal, lit, other, lit_depth, other_depth, disposition))
TACTIX_TRACE_FORWARD(sweep_equivalence, (const Kernel &k, Lit a, Lit b),
                     (k, a, b))
TACTIX_TRACE_FORWARD(sweep_commit, (const Kernel &k, Lit a, Lit b, int8_t av,
                     int8_t bv, const char *stage), (k, a, b, av, bv, stage))
TACTIX_TRACE_FORWARD(sweep_rewrite_binary, (const Kernel &k, Lit source,
                     Lit representative, uint32_t position, Lit other),
                     (k, source, representative, position, other))
TACTIX_TRACE_FORWARD(sweep_rewrite_occurrence, (const Kernel &k, Lit source,
                     Lit representative, uint32_t position, bool binary,
                     uint32_t payload),
                     (k, source, representative, position, binary, payload))
TACTIX_TRACE_FORWARD(sweep_unit, (const Kernel &k, Lit lit), (k, lit))
TACTIX_TRACE_FORWARD(factor_seed, (const Kernel &k, Lit seed, uint32_t score,
                     uint32_t remainders), (k, seed, score, remainders))
TACTIX_TRACE_FORWARD(factor_remainder, (const Kernel &k, Lit seed,
                     uint32_t ordinal, bool large, const Lit *lits,
                     uint32_t size), (k, seed, ordinal, large, lits, size))
TACTIX_TRACE_FORWARD(factor_candidate, (const Kernel &k, Lit seed,
                     uint32_t depth, uint32_t ordinal, Lit candidate,
                     uint32_t count, uint32_t score),
                     (k, seed, depth, ordinal, candidate, count, score))
TACTIX_TRACE_FORWARD(factor_literal_state, (const Kernel &k, Lit lit,
                     bool marked, bool retired, int value, uint32_t score),
                     (k, lit, marked, retired, value, score))
TACTIX_TRACE_FORWARD(factor_literal_entry, (const Kernel &k, Lit lit,
                     uint32_t ordinal, bool live, Lit other, uint64_t id,
                     const Clause *clause),
                     (k, lit, ordinal, live, other, id, clause))
TACTIX_TRACE_FORWARD(factor_candidate_source, (const Kernel &k, Lit seed,
                     uint32_t depth, uint32_t column, Lit scan, Lit candidate,
                     const Clause *current, const Clause *matched), (k, seed,
                     depth, column, scan, candidate, current, matched))
TACTIX_TRACE_FORWARD(factor_candidate_binary_source, (const Kernel &k, Lit seed,
                     uint32_t depth, uint32_t column, Lit q, Lit candidate),
                     (k, seed, depth, column, q, candidate))
TACTIX_TRACE_FORWARD(factor_mark_transition, (const Kernel &k, Lit lit,
                     bool marked, const char *source), (k, lit, marked, source))
TACTIX_TRACE_FORWARD(factor_update, (const Kernel &k, Lit grid_seed,
                     uint32_t ordinal, Lit lit, uint32_t score),
                     (k, grid_seed, ordinal, lit, score))
TACTIX_TRACE_FORWARD(factor_grid, (const Kernel &k, Lit seed, uint32_t factors,
                     uint32_t remainders, int delta),
                     (k, seed, factors, remainders, delta))
TACTIX_TRACE_FORWARD(factor_grid_member, (const Kernel &k, uint32_t ordinal,
                     Lit factor), (k, ordinal, factor))
TACTIX_TRACE_FORWARD(factor_fresh, (const Kernel &k, Lit seed, Var fresh,
                     const Lit *factors, uint32_t factor_count,
                     uint32_t remainders, int delta, uint64_t ticks,
                     uint64_t pops, uint64_t first_ticks, uint64_t next_ticks,
                     uint64_t match_ticks), (k, seed, fresh, factors,
                     factor_count, remainders, delta, ticks, pops, first_ticks,
                     next_ticks, match_ticks))
TACTIX_TRACE_FORWARD(bve_plan, (const Kernel &k, Var var, uint32_t side0,
                     uint32_t side1, uint32_t gate_kind, uint32_t gate0,
                     uint32_t gate1, uint64_t charged, bool resolve_gate), (k,
                     var, side0, side1, gate_kind, gate0, gate1, charged,
                     resolve_gate))
TACTIX_TRACE_FORWARD(bve_pop, (const Kernel &k, uint64_t ordinal,
                     uint64_t cumulative_hash, Var popped, double score,
                     Var root, uint32_t size),
                     (k, ordinal, cumulative_hash, popped, score, root, size))
TACTIX_TRACE_FORWARD(eliminate_cleanup, (const Kernel &k, const char *stage),
                     (k, stage))
TACTIX_TRACE_FORWARD(analyzed, (const Kernel &k,
                     const std::vector<Var> &analyzed,
                     const std::vector<uint8_t> &seen, uint8_t bump_mask),
                     (k, analyzed, seen, bump_mask))
TACTIX_TRACE_FORWARD(analyzed_post, (const Kernel &k,
                     const std::vector<Var> &analyzed), (k, analyzed))
TACTIX_TRACE_FORWARD(heap_scores, (const Kernel &k), (k))
TACTIX_TRACE_FORWARD(heap_insert, (const Kernel &k, Var v, uint32_t from,
                     uint32_t to), (k, v, from, to))
TACTIX_TRACE_FORWARD(heap_remove, (const Kernel &k, Var removed, Var last,
                     uint32_t removed_pos, uint32_t from, uint32_t to),
                     (k, removed, last, removed_pos, from, to))
TACTIX_TRACE_FORWARD(heap_pop, (const Kernel &k, const char *stage, Var held,
                     Var other, uint32_t from, uint32_t to),
                     (k, stage, held, other, from, to))
TACTIX_TRACE_FORWARD(binary_add, (const Kernel &k, Lit a, Lit b), (k, a, b))
TACTIX_TRACE_FORWARD(root_assign, (const Kernel &k, Lit lit), (k, lit))
TACTIX_TRACE_FORWARD(vivify_candidate, (const Kernel &k, const char *event,
                     uint64_t ordinal, Ref cref, uint64_t ticks,
                     uint64_t limit), (k, event, ordinal, cref, ticks, limit))
TACTIX_TRACE_FORWARD(vivify_counts, (const Kernel &k, const uint32_t *counts,
                     size_t size), (k, counts, size))
TACTIX_TRACE_FORWARD(randec_state, (const Kernel &k, const char *stage,
                     uint64_t remaining, uint64_t next, uint64_t sequence,
                     uint64_t rng), (k, stage, remaining, next, sequence, rng))
TACTIX_TRACE_FORWARD(substitute_map, (const Kernel &k, Lit lit,
                     Lit representative), (k, lit, representative))
#undef TACTIX_TRACE_FORWARD
