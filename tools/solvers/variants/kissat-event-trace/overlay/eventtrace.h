#ifndef _eventtrace_h_INCLUDED
#define _eventtrace_h_INCLUDED

#include <stdbool.h>
#include <stddef.h>

struct kissat;
struct clause;

void kissat_event_trace (struct kissat *, const char *, const char *, bool);
void kissat_event_trace_detail (struct kissat *, const char *, int);
void kissat_event_trace_literal (struct kissat *, const char *, unsigned);
void kissat_event_trace_watch_scan (struct kissat *, unsigned propagated);
void kissat_event_trace_implications (struct kissat *, size_t trail_begin);
void kissat_event_trace_conflict (struct kissat *, struct clause *);
void kissat_event_trace_clause (struct kissat *, const char *);
void kissat_event_trace_learned (struct kissat *);
void kissat_event_trace_checkpoint (struct kissat *);
void kissat_event_trace_close (void);

#endif
