#!/usr/bin/env python3
"""Read only the same censored 100K-clause prefix; never inspect its suffix."""
import itertools,json,lzma,sys
from pathlib import Path
input_path=Path(sys.argv[1]);out=Path(sys.argv[2]);source=[];alos=[];key_extent=None
with lzma.open(input_path,'rt') as stream:
    for line in stream:
        if not line.strip() or line.startswith(('c','p')):continue
        values=[int(x) for x in line.split()]
        assert values[-1]==0 and values.count(0)==1
        clause=values[:-1];source.append(clause)
        if key_extent is None:
            if len(clause)==4 and all(v>0 for v in clause) and clause==list(range(clause[0],clause[0]+4)):alos.append(clause)
            else:key_extent=4*len(alos)
        if len(source)==100000:break
binary={tuple(sorted(c)):cid for cid,c in enumerate(source,1) if len(c)==2}
reverse=[(cid,c) for cid,c in enumerate(source,1) if len(c)>2 and c[0]<-key_extent and all(0<v<=key_extent for v in c[1:])]
examples=[]
for cid,clause in reverse[:3]:
    output=-clause[0];inputs=clause[1:];domains=[]
    for did in sorted({(v-1)//4 for v in inputs}):
        values=alos[did];amo=[binary.get(tuple(sorted((-a,-b)))) for a,b in itertools.combinations(values,2)]
        assert all(amo)
        domains.append({'zero_based_domain':did,'values':values,'alo_occurrence':did+1,'amo_occurrences':amo})
    forwards=[binary.get(tuple(sorted((-v,output)))) for v in inputs];assert all(forwards)
    examples.append({'output':output,'inputs':inputs,'reverse_occurrence':cid,'forward_occurrences':forwards,'exact_input_domains':domains,'inputs_outside_original_64_domain_page':sum(v>256 for v in inputs),'auxiliary_outbound_binary_premises_in_prefix':[i for i,c in enumerate(source,1) if len(c)==2 and -output in c]})
result={'scope':'same first100000 original clauses as job1751725; suffix not inspected','source_prefix_clauses':len(source),'positive_contiguous_key_domain_candidates':len(alos),'key_external_extent':key_extent,'positive_auxiliary_or_reverse_clauses':len(reverse),'maximum_or_inputs_observed':max(len(c)-1 for _,c in reverse),'auxiliary_outbound_binary_premises_in_prefix':sum(len(c)==2 and c[0]<-key_extent for c in source),'first_auxiliary_or_examples':examples,'last_prefix_clause':source[-1],'source_suffix_complete':False,'domain_authentication_scope':'Only selected example domains and first64 domains from original census independently authenticated; candidate count is not full-domain acceptance.'}
out.write_text(json.dumps(result,indent=2)+'\n')
