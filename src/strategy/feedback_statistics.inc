// Deterministic native measurements: every kernel/stats.inc row except the
// nondeterministic `_nanos` wall timers, which remain in the snapshot. The
// suffix guards keep a mis-tagged row from replaying a wall timer.
#define TACTIX_STATS_NANOS(name)                                         \
  (sizeof(#name) > 6 && #name[sizeof(#name) - 7] == '_' &&               \
   #name[sizeof(#name) - 6] == 'n' && #name[sizeof(#name) - 5] == 'a' && \
   #name[sizeof(#name) - 4] == 'n' && #name[sizeof(#name) - 3] == 'o' && \
   #name[sizeof(#name) - 2] == 's')
#define TACTIX_STATS_SCALAR(name)                                        \
  static_assert(!TACTIX_STATS_NANOS(name), #name " is a wall timer");    \
  TACTIX_FEEDBACK_SCALAR(name)
#define TACTIX_STATS_U64(name) TACTIX_STATS_SCALAR(name)
#define TACTIX_STATS_U32(name) TACTIX_STATS_SCALAR(name)
#define TACTIX_STATS_MUTABLE(name) TACTIX_STATS_SCALAR(name)
#define TACTIX_STATS_INIT(name, value) TACTIX_STATS_SCALAR(name)
#define TACTIX_STATS_ARRAY(name, dims)                                   \
  static_assert(!TACTIX_STATS_NANOS(name), #name " is a wall timer");    \
  TACTIX_FEEDBACK_ARRAY(name)
#define TACTIX_STATS_TIMER(name)                                         \
  static_assert(TACTIX_STATS_NANOS(name), #name " is not a wall timer");
#define TACTIX_STATS_TIMER_ARRAY(name, dims) TACTIX_STATS_TIMER(name)
#include "kernel/stats.inc"
#undef TACTIX_STATS_NANOS
#undef TACTIX_STATS_SCALAR
#undef TACTIX_STATS_U64
#undef TACTIX_STATS_U32
#undef TACTIX_STATS_MUTABLE
#undef TACTIX_STATS_INIT
#undef TACTIX_STATS_ARRAY
#undef TACTIX_STATS_TIMER
#undef TACTIX_STATS_TIMER_ARRAY
