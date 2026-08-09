#ifndef _eventtrace_h_INCLUDED
#define _eventtrace_h_INCLUDED

#include <stdbool.h>

struct kissat;

void kissat_event_trace (struct kissat *, const char *, const char *, bool);
void kissat_event_trace_detail (struct kissat *, const char *, int);
void kissat_event_trace_literal (struct kissat *, const char *, unsigned);
void kissat_event_trace_checkpoint (struct kissat *);
void kissat_event_trace_close (void);

#endif
