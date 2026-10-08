/* Streaming statistics for binary DRAT/DSR proofs (r569 checker-memory study).
   Usage: proofstats PROOF   Prints one "c proofstats" line. Binary only. */
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  if (argc != 2) return 2;
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror("open"); return 2; }
  static char buf[1 << 20]; setvbuf(f, buf, _IOFBF, sizeof buf);
  unsigned long long adds = 0, dels = 0, add_lits = 0, del_lits = 0, wit_lines = 0, wit_lits = 0;
  unsigned long long unit_dels = 0, unit_adds = 0, empty_adds = 0, maxlen = 0, bytes = 0;
  long long live = 0, peak_live = 0, live_lits = 0, peak_live_lits = 0;
  int c;
  while ((c = getc(f)) != EOF) {
    bytes++;
    if (c != 'a' && c != 'd' && c != 1 && c != 2) { fprintf(stderr, "bad tag 0x%x at %llu\n", c, bytes); return 3; }
    int is_add = (c == 'a' || c == 1);
    unsigned long long len = 0, wl = 0; long long first = 0; int in_wit = 0; long long lits[64]; int nl = 0;
    for (;;) {
      unsigned long long v = 0; int shift = 0, b;
      do { b = getc(f); if (b == EOF) { fprintf(stderr, "truncated\n"); return 3; } bytes++; v |= (unsigned long long)(b & 0x7f) << shift; shift += 7; } while (b & 0x80);
      if (v == 0) break;
      long long lit = (v & 1) ? -(long long)(v >> 1) : (long long)(v >> 1);
      if (nl < 64) lits[nl++] = lit;
      if (in_wit) { wl++; continue; }
      if (is_add && len > 0 && lit == first) { in_wit = 1; wl++; continue; }
      if (len == 0) first = lit;
      len++;
    }
    if (is_add) {
      adds++; add_lits += len; if (len > maxlen) maxlen = len; if (len == 1) unit_adds++; if (len == 0) empty_adds++;
      if (in_wit) { wit_lines++; wit_lits += wl; if (wit_lines <= 5) { fprintf(stderr, "c witness-line addition=%llu deletions_before=%llu byte=%llu lits:", adds, dels, bytes); for (int i = 0; i < nl; i++) fprintf(stderr, " %lld", lits[i]); fprintf(stderr, "\n"); } }
      live++; live_lits += len;
      if (live > peak_live) peak_live = live; if (live_lits > peak_live_lits) peak_live_lits = live_lits;
    } else { dels++; del_lits += len; if (len == 1) unit_dels++; live--; live_lits -= len; }
  }
  printf("c proofstats bytes=%llu additions=%llu deletions=%llu added_lits=%llu deleted_lits=%llu avg_add_len=%.3f max_add_len=%llu unit_adds=%llu empty_adds=%llu unit_dels=%llu witness_lines=%llu witness_lits=%llu peak_live_proof_clauses=%lld peak_live_proof_lits=%lld final_live_proof_clauses=%lld\n",
    bytes, adds, dels, add_lits, del_lits, adds ? (double) add_lits / adds : 0.0, maxlen, unit_adds, empty_adds, unit_dels, wit_lines, wit_lits, peak_live, peak_live_lits, live);
  return 0;
}
