#pragma once
#include "session/query_history.hpp"
#include <csignal>
#include <string>
namespace tactix::session {
enum class QueryCertificateStatus { Unchanged, Written, Interrupted, Error };
// A post-capture certificate producer, independent of the native strategy and
// its transformed representation. For an ASCII DSR query whose exact raw F+A
// is propagation-inconsistent, publish its direct empty-clause RUP proof.
// This avoids an ambiguous first nonempty lemma in pinned DSR elaboration of
// already-refuted inputs. No native rule, heuristic, trail or result changes.
// Other inputs and PBP output retain their existing certificate unchanged.
QueryCertificateStatus write_query_up_certificate(const QueryHistory &,uint64_t query,
    const std::string &prefix,const volatile sig_atomic_t *stop=nullptr);
}
