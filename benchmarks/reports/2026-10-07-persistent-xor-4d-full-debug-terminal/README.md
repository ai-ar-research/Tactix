# 4d full Debug terminal evidence — 2026-10-07

Job `1752523` terminated FAILED on the frozen committed `4d7069a4c3ca8342a96ad93b1409093a83291ba1` source. CTest ran all 338 tests: 336 passed; `comment_hygiene` and `doc_size` failed. This is not full-Debug acceptance.

The comment failure is a test comment pointing to an existing ignored completed fuzz report which was absent from the archived source package. The future harness dependency list is repaired to package that exact report; neither the old archive nor its results are altered. The failing report path and current content hash are recorded in `summary.json`. The four document budgets remain exceeded and unwaived: development plan 751/750, winner integration plan 249/240, calculus 683/680, implementation notes 570/560. No automatic compaction or threshold relaxation is performed.

The manifest binds the archived source, 273 executables and 387 build bindings. Native solver, API/replay and proof/model tests passed within this run; subsequent uncommitted XOR increments still require their own full campaign. Raw terminal CTest output, JUnit and scheduler log are frozen here.
