#pragma once

// Effort records, cumulative Stats and feedback additions share field order.
// Rows are (native type, member); records start at zero, Stats stay uint64_t.
#define TACTIX_DECLARE_EFFORT_FIELD(Type, Name) Type Name = 0;
#define TACTIX_DECLARE_EFFORT_FIELDS(Family) \
  TACTIX_EFFORT_SCHEMA_##Family(TACTIX_DECLARE_EFFORT_FIELD)

#define TACTIX_EFFORT_SCHEMA_TseitinCycleBreakingEffort(FIELD) \
  FIELD(uint64_t, snapshot_clauses) \
  FIELD(uint64_t, snapshot_literals) \
  FIELD(uint64_t, snapshot_work) \
  FIELD(uint64_t, recapture_work) \
  FIELD(uint64_t, discovery_work) \
  FIELD(uint64_t, retained_bytes) \
  FIELD(uint64_t, admission_work) \
  FIELD(uint64_t, planning_work) \
  FIELD(uint64_t, transaction_work) \
  FIELD(uint64_t, validation_ticks) \
  FIELD(uint64_t, candidates) \
  FIELD(uint64_t, offers) \
  FIELD(uint64_t, identity_bytes) \
  FIELD(uint64_t, identity_work) \
  FIELD(uint64_t, journal_work) \
  FIELD(uint64_t, journal_bytes) \
  FIELD(uint64_t, session_work) \
  FIELD(uint64_t, session_identity_bytes) \
  FIELD(uint64_t, proof_additions) \
  FIELD(uint64_t, proof_deletions) \
  FIELD(uint64_t, proof_literal_occurrences) \
  FIELD(uint64_t, proof_helpers) \
  FIELD(uint64_t, proof_only_variables) \
  FIELD(uint64_t, proof_witness_literals) \
  FIELD(uint64_t, proof_routes)

#define TACTIX_EFFORT_SCHEMA_Gf3AffineActionEffort(FIELD) \
  FIELD(uint64_t, source_clauses) \
  FIELD(uint64_t, source_literals) \
  FIELD(uint64_t, equations) \
  FIELD(uint64_t, owner_capture_work) \
  FIELD(uint64_t, recognition_work) \
  FIELD(uint64_t, planning_work) \
  FIELD(uint64_t, duplicate_scan_work) \
  FIELD(uint64_t, revalidation_work) \
  FIELD(uint64_t, proof_work) \
  FIELD(uint64_t, planned_additions) \
  FIELD(uint64_t, duplicate_additions) \
  FIELD(uint64_t, emitted_additions) \
  FIELD(uint64_t, proof_literal_occurrences) \
  FIELD(uint64_t, arena_words) \
  FIELD(uint64_t, journal_work) \
  FIELD(uint64_t, journal_bytes) \
  FIELD(uint64_t, session_work)

#define TACTIX_EFFORT_SCHEMA_DirectCspSupportActionEffort(FIELD) \
  FIELD(uint64_t, source_clauses) \
  FIELD(uint64_t, source_literals) \
  FIELD(uint64_t, owner_capture_work) \
  FIELD(uint64_t, recognition_work) \
  FIELD(uint64_t, source_scan_work) \
  FIELD(uint64_t, planning_work) \
  FIELD(uint64_t, revalidation_work) \
  FIELD(uint64_t, proof_work) \
  FIELD(uint64_t, planned_additions) \
  FIELD(uint64_t, emitted_additions) \
  FIELD(uint64_t, proof_literal_occurrences) \
  FIELD(uint64_t, journal_work) \
  FIELD(uint64_t, journal_bytes) \
  FIELD(uint32_t, extent_growth) \
  FIELD(uint64_t, session_work)
