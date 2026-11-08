#pragma once
#include "session/result_snapshot.hpp"
#include <string>
namespace tactix::session {
// A reduced core needs its own exact F+C theorem, independent of the proof of
// the larger query F+A. Only F-licensed recipes enter this certificate.
bool write_core_certificate(const QueryHistory &,const QueryResultSnapshot &,
                            const std::string &prefix);
}
