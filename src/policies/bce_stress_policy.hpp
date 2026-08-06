#pragma once

#include "kernel/kernel.hpp"

#include <csignal>
#include <cstdint>

namespace tactix {

// Temporary N1 stress policy (X1 BCE, stress-policy skill): maximizes BLOCKED
// CLAUSE ELIMINATION, and with it the WITNESS segments BCE pushes onto ρ.
//
// Why this exists. BCE's failure mode is not a rejected proof — a deletion line
// is accepted unconditionally by every checker — it is a WRONG SAT ANSWER: the
// deletion only preserves satisfiability, so a missing, mis-scoped or
// mis-ordered witness segment silently corrupts model reconstruction, and no
// proof checker can see it. That is verbatim the justification the CCE stress
// policy gives for its own existence, and BCE had no equivalent driver
// (`RV-S22` in benchmarks/reports/2026-08-06-review-post-track-sched.md). The
// oracle for this policy is therefore the MODEL check on every SAT verdict, not
// the proof check, so the policy must reach SAT verdicts to be worth anything.
//
// The schedules it drives, all legal, none reached often by chance:
//  (1) BCE AT EVERY ROOT, to a fixpoint, with `work_budget` left at 0 so each
//      apply runs the WHOLE round. Round n's deletions change which clauses
//      round n+1's tautology test quantifies over — the blockedness argument is
//      monotone under deletion, and this is what exercises that.
//  (2) THE PER-LITERAL INSTANCE SURFACE (`bce_try` via apply_instance), drawn
//      at random rather than only the canonical round: it is the N1 path a
//      learned policy uses, and one literal's deletions can unblock another's.
//  (3) INTERLEAVED WITH THE OTHER WITNESS-PUSHING RULES — E1 BVE, X3 CCE and V3
//      Substitute — so ρ carries BCE's segments stacked between other rules'
//      segments, in both orders. Witness ORDER across rules is the live
//      question: ρ is replayed in reverse, and a segment that is satisfied only
//      because a LATER-pushed segment forced its witness is exactly the case
//      test_reconstruct pins for one rule and nothing pins across rules.
//  (4) BCE OFF A PROPAGATION FIXPOINT (the root branch sits before the
//      propagate branch), which the guard permits and DefaultPolicy never does.
//
// Verdicts come from a simple random DPLL search with real conflict analysis, so
// the model oracle fires and there are redundant clauses around that BCE must
// NOT delete (deviation 1 in bce.hpp). Per-step `kernel_check_invariants` (not
// the every-256 default) pins any culprit. Knobs: TACTIX_BCE_STRESS_ROUNDS
// (canonical rounds per root visit, default 4), TACTIX_BCE_STRESS_INSTANCES
// (per-literal draws per root visit, default 4), TACTIX_BCE_STRESS_SOLO (drop
// the BVE/CCE/Substitute interleave), TACTIX_TRACE (per-step trace).
int bce_stress_solve(Kernel &k, uint64_t seed, volatile sig_atomic_t *stop);

} // namespace tactix
