#!/usr/bin/env python3
"""Analyze retained result only; never opens the original target formula."""
import itertools,json,math
from pathlib import Path
HERE=Path(__file__).resolve().parent
r=json.loads((HERE/'artifacts/census.json').read_text())
prefix=json.loads((HERE/'artifacts/prefix-review.json').read_text())
neighbors=r['outgoing_neighbors'];magnitudes=sorted(abs(n['signed_literal']) for n in neighbors)
seed=next(d for d in r['authenticated_or_definitions'] if d['output']==r['seed'])
stride=math.gcd(*(x-seed['inputs'][0] for x in seed['inputs'][1:]))
keymax=prefix['key_external_extent'];block_base=keymax+prefix['positive_auxiliary_or_reverse_clauses']+1
seed_keys=[(x-1)//stride for x in seed['inputs']]
candidate_key_count=keymax//stride
expected={block_base+k*stride for k in range(candidate_key_count) if k not in seed_keys}
assert set(magnitudes)==expected
owners={x:i for i,d in enumerate(r['selected_domains']) for x in d['values']}
eligible=sum(owners[a]!=owners[c] and owners[a]!=mid and owners[c]!=mid for mid in range(len(r['selected_domains'])) for a,c in itertools.combinations(owners,2))
raw_pairs=len(r['selected_domains'])*math.comb(len(owners),2)
assert eligible==480 and raw_pairs==950 and not r['helpers'] and not r['path_nogoods']
analysis={
 'source':'retained census and frozen prefix review only; no target reread',
 'input_hash_complete':r['input_hash_complete'],'source_complete':r['source_complete'],'query_complete':r['query_complete'],
 'outgoing_relations':len(neighbors),'negative_auxiliary_relations':sum(n['signed_literal']<0 and abs(n['signed_literal'])>keymax for n in neighbors),
 'positive_relations':sum(n['signed_literal']>0 for n in neighbors),
 'neighbor_min':min(magnitudes),'neighbor_max':max(magnitudes),
 'premise_min':min(n['premise'] for n in neighbors),'premise_max':max(n['premise'] for n in neighbors),
 'neighbor_gap_exceptions':[[a,b] for a,b in zip(magnitudes,magnitudes[1:]) if b-a!=stride],
 'authenticated_seed_definitions':1,'authenticated_neighbor_definitions':len(r['authenticated_or_definitions'])-1,
 'selected_exact_domains':len(r['selected_domains']),'selected_values':len(owners),'lifted_helpers':len(r['helpers']),'proposed_nogoods':len(r['path_nogoods']),
 'raw_middle_endpoint_pair_iterations':raw_pairs,'distinct_three_owner_combinations':eligible,
 'owner_excluded_iterations':raw_pairs-eligible,
 'decisive_no_yield':'All neighbor definition lookups returned null within the admitted catalog/grammar. No lifted helper exists; every covered path would fail the mandatory uses_lift condition.',
 'unavailable_counters':['actual support-cardinality histogram','covered vs uncovered path counts','original duplicate exclusions','binary-reachability exclusions','missing reverse vs missing forward vs outside catalog grammar for each neighbor'],
 'unavailable_counter_interpretation':'Not retained by this tool; cannot infer individual branch counts or support histograms from empty output. No target scan performed to reconstruct them.',
 'reference_consistent_index_pattern':{'seed_input_difference_gcd':stride,'candidate_P':stride//4,'candidate_K':candidate_key_count,'candidate_L':prefix['positive_auxiliary_or_reverse_clauses']//stride,'block_base':block_base,'excluded_seed_key_indices':seed_keys,'neighbors_exactly_match_other_keys':True,'semantic_authentication':'Index arithmetic only; blocker definitions/cut ALO and original premise interpretation have not been authenticated.'},
 'gate':'not_admitted; no native implementation or longer solver probe justified',
 'negative_scope':'Complete ingestion and complete query of one seed using exact prefix key domains and positive-key-input OR definitions. Does not reject blocker/cut semantics, other owners, higher-arity mechanisms, or DirectCSP as a whole.'}
(HERE/'analysis.json').write_text(json.dumps(analysis,indent=2)+'\n')
print('PASS: retained-result arithmetic and no-helper admission boundary')
