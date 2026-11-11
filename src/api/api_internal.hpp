#pragma once
#include "session/query_result.hpp"
#include "strategy/online_sequence.hpp"
#include <csignal>
#include <string>
#include <utility>
#include <vector>

// Internal access for fixed tests and api_work.cpp's additive entry points;
// this is not part of the public C interface.
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace tactix {
struct Kernel;
struct Stats;
struct DefaultStrategyConfig;

class DefaultPolicy;
namespace session {class QueryKernel;}
namespace strategy {
struct OnlineStrategyProgram;
class BveCandidateHeap;
class Allowance;
class RegionProgram;
}
namespace api_detail {
Kernel &kernel_for_fixed_testing(void *opaque);
// The configured online program as converted from C, or null.
const strategy::OnlineStrategyProgram *online_program_for_fixed_testing(void *opaque);
// Finalize the session's input (as every reasoning entry point does).
void complete_input(void *opaque);
// Session storage that tactix_read_parameters' pointer fields refer to.
std::shared_ptr<void> &observe_scratch(void *opaque);
// The session's default-strategy configuration (tactix_strategy_*); fixed
// tests pose another through default_policy_configuration.
DefaultStrategyConfig &strategy_configuration(void *opaque);
// The session's open BVE schedule (tactix_bve_schedule_*), or null.
std::unique_ptr<strategy::BveCandidateHeap> &bve_schedule(void *opaque);
// The configured structured-root stage and its SR-discovery allowance.
std::unique_ptr<strategy::OnlineStrategySequence> &structured_stage(void *opaque);
std::unique_ptr<strategy::Allowance> &sr_discovery(void *opaque);
// The recorded affine source census (TACTIX_SUPPORT_AFFINE_CENSUS).
std::optional<std::array<uint64_t, 13>> &affine_census(void *opaque);
std::array<uint64_t, 13> affine_census_stamp(void *opaque);
// The session's region program (tactix_regions_begin), or null.
strategy::RegionProgram *regions(void *opaque);
// Queued region queries not yet run.
size_t regions_pending(void *opaque);
// The tick-attribution stats (tactix_install_tick_attribution) and their
// counters: null / TACTIX_TICK_ATTRIBUTION_NONE outside the table.
uint64_t Stats::*tick_attribution_counter(int stat);
int tick_attribution_stat(uint64_t Stats::*counter);

const strategy::OnlineStrategyProgram *online_executor_for_fixed_testing(void *opaque);
// A cold execution view borrows an active query's Kernel/H/proof/services.
// It must be destroyed before the query is finished or its epoch changes.
void *create_query_execution(session::QueryKernel &,DefaultPolicy &,
    std::vector<std::pair<std::string,std::string>> &,volatile sig_atomic_t *);
void destroy_query_execution(void *);
bool query_execution_settled(void *);
session::QuerySolveStatus query_execution_status(void *);
int set_strategy_option(std::vector<std::pair<std::string,std::string>> &,
    const char *name,const char *value);
}
}
