#!/usr/bin/env python3
"""Price reviewed assignment bookkeeping sites in the cached sampled image."""
from pathlib import Path
source = Path(__file__).with_name('summarize-retention-6531.py')
code = source.read_text()
for old, new in {
    '20260909-6530-rank32-retention-annotation': '20260909-6604-rank32-propagation-annotation',
    'summary-6531.json': 'summary-6608.json',
    'summarize-retention-6531.py': 'summarize-propagation-6608.py',
    'combined_two_retention_sites_percent_program': 'combined_search_phase_save_sites_percent_program',
    "('rebuild_retention_and_list_bookkeeping', 'collection_pure_retention_and_metadata')": "('binary_phase_save', 'large_phase_save')",
    'Deprioritize the scan-only proposal as the main Rank32 gap-closing step. Keep baseline contract tests; no implementation or new timing is selected by this attribution.':
    'Do not select an assignment phase-saving specialization as the main gap-closing step from this small sampled share. Keep search/probing/public contracts. The aggregate search symbol is not slower per operation merely because it leads candidate samples; continue inprocessing/search-quality localization, including largest UNSAT rank31.',
    'One sampled pair; instruction skid and frequency-weighted sampling apply. Region shares are not time savings, independent ablations or hardware-stall causes. Mixed-list and bookkeeping work remains after a fast path. Drop-large caller coverage is not measured here; do not claim a complete three-site ceiling. Native scopes differ. Source equality does not establish new linked-image cost.':
    'One cached sampled pair; instruction skid and event-period weighting apply. Regions include useful phase saving and are not removable overhead or predicted savings. Other assignment work is not covered by these two sites. Candidate probing annotation is the wrapper, not its 4.61%-self propagate_literal callee; no probing-cost ceiling. Native function/work scopes differ. Source-section equality to6579 does not establish identical current machine code or per-operation cost. Full measured inprocessing time122957ms motivates continued owner-level diagnosis.',
}.items():
    assert code.count(old) == (2 if old == 'combined_two_retention_sites_percent_program' else 1), old
    code = code.replace(old, new)
begin, end = code.index('symbols = {'), code.index('\nfunctions, parsed =')
code = code[:begin] + '''symbols = {
    'candidate-search': 'tactix::kernel_propagate(tactix::Kernel&, unsigned int)',
    'candidate-probing': 'tactix::kernel_propagate_probing(tactix::Kernel&, unsigned int)',
    'kissat-search': 'kissat_search_propagate',
    'kissat-probing': 'kissat_probing_propagate',
}''' + code[end:]
begin, end = code.index('ranges = {'), code.index('\nregions = {}')
code = code[:begin] + '''ranges = {
    'binary_phase_save': ('candidate-search', [(0x413997, 0x4139cb)]),
    'large_phase_save': ('candidate-search', [(0x4142ef, 0x414330)]),
    'watch_decode_and_blocker': ('candidate-search', [(0x413a09, 0x413a39)]),
    'large_clause_load_and_garbage_branch': ('candidate-search', [(0x413a39, 0x413a58)]),
}''' + code[end:]
exec(compile(code, str(source), 'exec'), {'__file__': str(source), '__name__': '__main__'})
