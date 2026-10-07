#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <lzma.h>
#include <openssl/evp.h>

// Standalone original-clause census. No solver kernel or model transformation.
using I=int32_t; using U=uint32_t;
struct Binary {I a,b;U id;};
struct Edge {I to;U id;};
struct Clause {U id;std::vector<I> v;};
struct Helper {I a,b;std::vector<U> ids;};
struct Stop:std::runtime_error {using std::runtime_error::runtime_error;};
struct Tool {
 std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
 double seconds=30; uint64_t query_cap=10000000,query_work=0,bytes=0,compressed=0;
 U variables=0,clauses=0,occurrences=0,keymax=0;I seed=10145;
 uint64_t sort_work=0,sort_cap=0;
 bool query_complete=false;
 bool complete=false,hash_complete=false,prefix=true,header=false;std::string hash,status="incomplete";
 std::vector<Binary> binary;std::vector<Clause> domains,defs,covers,possible_existing;
 I seed_blocker=17249;U authorized_domain_begin=0;std::vector<U> evidence_ids;std::string cover_description="null",support_description="null";std::vector<U> certificate_masks;
 struct Reason {I from=0;U id=0;};
 std::vector<U> offsets;std::vector<Edge> edges;
 std::vector<Clause> authenticated_defs;std::vector<Edge> outgoing;
 std::set<std::pair<I,I>> helper_keys;
 std::vector<Helper> helpers;std::vector<std::string> offers;
 std::map<I,size_t> owners;std::vector<size_t> selected;
 uint64_t metadata_bytes=0;static constexpr uint64_t metadata_cap=32ULL<<20;
 void wall(){if(std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()>=seconds)throw Stop("wall_bound");}
 void charge(uint64_t n=1){wall();if(n>query_cap-query_work)throw Stop("query_bound");query_work+=n;}
 static U node(I x){return 2*U(std::abs(int64_t(x))-1)+(x<0);}
 static void order(I&a,I&b){if(a>b)std::swap(a,b);}
 U find(I a,I b){charge();order(a,b);auto it=std::lower_bound(binary.begin(),binary.end(),Binary{a,b,0},[&](auto x,auto y){charge();return std::tie(x.a,x.b,x.id)<std::tie(y.a,y.b,y.id);});return it!=binary.end()&&it->a==a&&it->b==b?it->id:0;}
 void retain(std::vector<Clause>&dest,U id,const std::vector<I>&v){uint64_t n=sizeof(Clause)+v.size()*sizeof(I);if(n>metadata_cap-metadata_bytes)throw Stop("metadata_bound");metadata_bytes+=n;dest.push_back({id,v});}
 void consume(std::vector<I>&v){wall();if(occurrences==clauses)throw std::runtime_error("too_many_clauses");U id=++occurrences;
  std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());
  bool taut=false;for(I x:v)if(std::binary_search(v.begin(),v.end(),-x))taut=true;
  bool positive=!v.empty()&&v.front()>0;
  if(prefix&&positive&&v.size()>=2&&v.size()<=64){retain(domains,id,v);keymax=std::max(keymax,U(v.back()));}else prefix=false;
  if(!taut&&positive&&v.size()==32&&std::binary_search(v.begin(),v.end(),seed_blocker))retain(covers,id,v);
  if(!taut&&v.size()==16&&v.back()<0&&uint64_t(-int64_t(v.front()))<=keymax)retain(possible_existing,id,v);
  if(!taut&&v.size()==2)binary.push_back({v[0],v[1],id});
  if(!taut&&v.size()>=3&&v.size()<=65&&v.front()<0&&v[1]>0&&U(v.back())<=keymax&&U(-int64_t(v.front()))>keymax)retain(defs,id,v);
  v.clear();
 }
 void parse(const std::string&path,const std::string&expected,U expected_v,U expected_m,uint64_t decoded_cap,uint64_t compressed_cap){
  std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("input_open");
  EVP_MD_CTX* md=EVP_MD_CTX_new();if(!md||EVP_DigestInit_ex(md,EVP_sha256(),nullptr)!=1)throw std::runtime_error("sha_init");
  lzma_stream z=LZMA_STREAM_INIT;auto init=lzma_stream_decoder(&z,16ULL<<20,0);if(init!=LZMA_OK)throw std::runtime_error("decoder_init");
  std::array<unsigned char,65536> ib{},ob{};bool eof=false,ended=false,line_start=true,comment=false,head=false;std::string token,hline;std::vector<I> pending;
  auto token_end=[&](){if(token.empty())return;size_t used=0;long long val=std::stoll(token,&used);if(used!=token.size()||val<INT32_MIN+1LL||val>INT32_MAX)throw std::runtime_error("bad_integer");token.clear();if(!header)throw std::runtime_error("missing_header");if(val==0)consume(pending);else {if(uint64_t(std::abs(val))>variables)throw std::runtime_error("literal_outside_header");if(pending.size()>=variables)throw Stop("clause_width_bound");pending.push_back(I(val));}};
  auto header_end=[&](){std::istringstream hs(hline);std::string p,cnf,extra;uint64_t vv,mm;if(header||!(hs>>p>>cnf>>vv>>mm)||p!="p"||cnf!="cnf"||(hs>>extra)||!vv||vv>INT32_MAX/2||mm>UINT32_MAX||vv!=expected_v||mm!=expected_m)throw std::runtime_error("header_binding");variables=U(vv);clauses=U(mm);binary.reserve(clauses);header=true;hline.clear();};
  while(!ended){wall();if(!z.avail_in&&!eof){in.read(reinterpret_cast<char*>(ib.data()),ib.size());auto n=in.gcount();if(in.bad())throw std::runtime_error("input_read");if(n){if(uint64_t(n)>compressed_cap-compressed)throw Stop("compressed_byte_bound");EVP_DigestUpdate(md,ib.data(),size_t(n));compressed+=uint64_t(n);z.next_in=ib.data();z.avail_in=size_t(n);}else eof=true;}
   z.next_out=ob.data();z.avail_out=ob.size();auto rc=lzma_code(&z,eof?LZMA_FINISH:LZMA_RUN);size_t n=ob.size()-z.avail_out;if(n>decoded_cap-bytes)throw Stop("decoded_byte_bound");bytes+=n;
   for(size_t j=0;j<n;++j){if(!(j&4095))wall();char c=char(ob[j]);if(comment){if(c=='\n'){comment=false;line_start=true;}continue;}if(head){if(c=='\n'){header_end();head=false;line_start=true;}else {if(hline.size()>=256)throw std::runtime_error("header_width");hline+=c;}continue;}
    if(c==' '||c=='\t'||c=='\r'||c=='\n'){token_end();if(c=='\n')line_start=true;continue;}
    if(line_start&&(c=='c'||c=='p')){if(!pending.empty())throw std::runtime_error("directive_in_clause");if(c=='c')comment=true;else {head=true;hline="p";}line_start=false;continue;}line_start=false;if((c<'0'||c>'9')&&c!='-'&&c!='+')throw std::runtime_error("bad_character");if(token.size()>=12)throw std::runtime_error("integer_width");token+=c;
   }
   if(rc==LZMA_STREAM_END){ended=true;if(z.avail_in||in.peek()!=EOF)throw std::runtime_error("trailing_compressed_data");}
   else if(rc==LZMA_MEM_ERROR)throw Stop("allocation_bound");
   else if(rc==LZMA_MEMLIMIT_ERROR)throw Stop("decoder_memory_bound");
   else if(rc!=LZMA_OK)throw std::runtime_error("xz_integrity_or_eof");
  }
  if(head){header_end();}
  token_end();if(!pending.empty()||!header||occurrences!=clauses)throw std::runtime_error("clause_count_or_eof");
  unsigned char digest[32];unsigned len=0;EVP_DigestFinal_ex(md,digest,&len);EVP_MD_CTX_free(md);lzma_end(&z);std::ostringstream ss;for(unsigned j=0;j<len;++j)ss<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(digest[j]);hash=ss.str();hash_complete=true;if(hash!=expected)throw std::runtime_error("input_hash_binding");complete=true;
 }
 const Clause* definition(I q){charge();for(auto&d:defs){charge();if(d.v[0]==-q){bool exact=true;for(size_t i=1;i<d.v.size();++i)if(!find(-d.v[i],q)){exact=false;break;}if(exact)return &d;}}return nullptr;}
 bool add_domain(I x){for(size_t i=0;i<domains.size();++i){charge();auto&d=domains[i];if(!std::binary_search(d.v.begin(),d.v.end(),x))continue;bool exact=true;for(size_t a=0;a<d.v.size();++a)for(size_t b=a+1;b<d.v.size();++b)if(!find(-d.v[a],-d.v[b]))exact=false;if(!exact)continue;
   if(std::find(selected.begin(),selected.end(),i)!=selected.end()){return true;}
   if(selected.size()==64){throw Stop("owner_bound");}
   for(I v:d.v){if(owners.count(v))throw Stop("overlapping_owner");}
   selected.push_back(i);for(I v:d.v)owners[v]=i;return true;}return false;}
 void index(){wall();unsigned log=0;for(uint64_t n=binary.size();n;n>>=1)++log;sort_cap=2ULL*binary.size()*(log+1);std::sort(binary.begin(),binary.end(),[&](auto a,auto b){if(++sort_work>sort_cap)throw Stop("sort_work_bound");if(!(sort_work&4095))wall();return std::tie(a.a,a.b,a.id)<std::tie(b.a,b.b,b.id);});wall();binary.erase(std::unique(binary.begin(),binary.end(),[](auto a,auto b){return a.a==b.a&&a.b==b.b;}),binary.end());
  uint64_t nn=2ULL*variables,ee=2ULL*binary.size();if(ee>UINT32_MAX||nn+1>SIZE_MAX/sizeof(U))throw Stop("index_arithmetic_bound");offsets.assign(size_t(nn+1),0);for(size_t i=0;i<binary.size();++i){if(!(i&4095))wall();++offsets[node(-binary[i].a)+1];++offsets[node(-binary[i].b)+1];}for(size_t i=1;i<offsets.size();++i){if(!(i&4095))wall();offsets[i]+=offsets[i-1];}auto cursor=offsets;edges.resize(size_t(ee));for(size_t i=0;i<binary.size();++i){if(!(i&4095))wall();auto b=binary[i];edges[cursor[node(-b.a)]++]={b.b,b.id};edges[cursor[node(-b.b)]++]={b.a,b.id};}}
 bool reachable(I a,I target){std::vector<unsigned char> seen(2ULL*variables,0);std::vector<I> todo{a};seen[node(a)]=1;while(!todo.empty()){I x=todo.back();todo.pop_back();for(U i=offsets[node(x)];i<offsets[node(x)+1];++i){charge();I y=edges[i].to;if(y==target)return true;if(!seen[node(y)]){seen[node(y)]=1;todo.push_back(y);}}}return false;}
 static std::string list(const std::vector<U>&v){std::ostringstream s;s<<'[';for(size_t i=0;i<v.size();++i){if(i)s<<',';s<<v[i];}return s.str()+']';}
 bool binary_conflict(const std::vector<I>&candidate){
  std::vector<unsigned char> seen(2ULL*variables,0);std::vector<I> todo;
  auto add=[&](I x){charge();if(seen[node(-x)])return false;if(!seen[node(x)]){seen[node(x)]=1;todo.push_back(x);}return true;};
  for(I x:candidate)if(!add(-x))return true;
  while(!todo.empty()){I x=todo.back();todo.pop_back();for(U i=offsets[node(x)];i<offsets[node(x)+1];++i){charge();if(!add(edges[i].to))return true;}}
  return false;
 }
 U existing(const std::vector<I>&candidate){for(auto&c:possible_existing){charge();if(c.v==candidate)return c.id;}return 0;}
 void query(){
  if(!find(-seed,-seed_blocker)){status="missing_selected_seed_link";return;}
  if(covers.size()!=1){status="missing_or_ambiguous_cover";return;}
  const Clause&cover=covers.front();evidence_ids.push_back(cover.id);
  struct Block {I b,x,q;U xid,qid;const Clause*definition;};std::vector<Block> blocks;
  std::map<size_t,std::map<I,size_t>> rows;
  for(I b:cover.v){charge();I x=0,q=0;U xid=0,qid=0;
   for(U k=offsets[node(b)];k<offsets[node(b)+1];++k){charge();auto e=edges[k];if(e.to>0&&U(e.to)<=keymax){if(x&&x!=e.to){status="ambiguous_key_implication";return;}x=e.to;xid=e.id;}if(e.to<0&&uint64_t(-int64_t(e.to))>keymax){if(q&&q!=-e.to){status="ambiguous_lock_implication";return;}q=-e.to;qid=e.id;}}
   if(!x||!q||!add_domain(x)){status="missing_blocker_implication_or_domain";return;}
   const Clause*def=definition(q);if(!def){status="missing_lock_or_definition";return;}
   auto&domain=domains[owners[x]];if(domain.v.size()!=4){status="wrong_domain_width";return;}
   if(!rows[owners[x]].emplace(x,blocks.size()).second){status="duplicate_blocker_coordinate";return;}
   if(b==seed_blocker&&q!=seed){status="selected_seed_role_mismatch";return;}
   blocks.push_back({b,x,q,xid,qid,def});evidence_ids.push_back(xid);evidence_ids.push_back(qid);authenticated_defs.push_back(*def);
  }
  if(rows.size()!=8){status="wrong_position_count";return;}
  if(uint64_t(authorized_domain_begin)+rows.size()>domains.size()){status="missing_authorized_row";return;}
  std::vector<size_t> unauthorized,authorized;
  for(auto&row:rows){if(row.second.size()!=4){status="incomplete_blocker_grid";return;}unauthorized.push_back(row.first);}
  for(size_t p=0;p<8;++p){size_t ai=authorized_domain_begin+p;if(domains[ai].v.size()!=4||!add_domain(domains[ai].v.front())){status="missing_authorized_domain";return;}if(rows.count(ai)){status="overlapping_rows";return;}authorized.push_back(ai);}
  std::map<I,I> partner;
  for(size_t p=0;p<8;++p){std::set<I> used;
   for(I x:domains[unauthorized[p]].v){auto&b=blocks[rows[unauthorized[p]][x]];I y=0;for(size_t j=1;j<b.definition->v.size();++j){charge();I value=b.definition->v[j];if(std::binary_search(domains[authorized[p]].v.begin(),domains[authorized[p]].v.end(),value)){if(y){status="ambiguous_authorized_or_input";return;}y=value;}}
    if(!y||!used.insert(y).second){status="incomplete_authorized_value_matching";return;}partner[x]=y;
   }
  }
  std::ostringstream desc;desc<<"{\"alo\":"<<cover.id<<",\"blockers\":[";for(size_t i=0;i<blocks.size();++i){if(i)desc<<',';auto&b=blocks[i];desc<<"{\"blocker\":"<<b.b<<",\"key_literal\":"<<b.x<<",\"lock_literal\":"<<b.q<<",\"key_implication\":"<<b.xid<<",\"negative_lock_implication\":"<<b.qid<<'}';}desc<<"],\"unauthorized_domain_alos\":[";for(size_t p=0;p<8;++p){if(p)desc<<',';desc<<domains[unauthorized[p]].id;}desc<<"],\"authorized_domain_alos\":[";for(size_t p=0;p<8;++p){if(p)desc<<',';desc<<domains[authorized[p]].id;}desc<<"]}";cover_description=desc.str();
  // Prefix selection uses actual selected-key incompatibilities, not depths.
  std::set<std::pair<I,I>> incompat;
  for(auto ai=owners.begin();ai!=owners.end();++ai)for(auto bi=std::next(ai);bi!=owners.end();++bi)if(find(-ai->first,-bi->first))incompat.insert({ai->first,bi->first});
  auto compatible=[&](I a,I b){charge();if(a>b)std::swap(a,b);return !incompat.count({a,b});};
  std::vector<I> fixedx,fixedy,eligiblex,eligibley;
  std::function<bool(size_t)> choose=[&](size_t p){
   if(p==7){eligiblex.clear();eligibley.clear();for(I v:domains[unauthorized.back()].v){bool ok=true;for(I x:fixedx)if(!compatible(v,x))ok=false;for(I y:fixedy)if(!compatible(v,y))ok=false;if(ok)eligiblex.push_back(v);}for(I v:domains[authorized.back()].v){bool ok=true;for(I x:fixedx)if(!compatible(v,x))ok=false;for(I y:fixedy)if(!compatible(v,y))ok=false;if(ok)eligibley.push_back(v);}return eligiblex.size()>=3&&eligibley.size()>=3;}
   for(I x:domains[unauthorized[p]].v){I y=partner[x];bool ok=compatible(x,y);for(I v:fixedx)if(!compatible(x,v)||!compatible(y,v))ok=false;for(I v:fixedy)if(!compatible(x,v)||!compatible(y,v))ok=false;if(!ok)continue;fixedx.push_back(x);fixedy.push_back(y);if(choose(p+1))return true;fixedx.pop_back();fixedy.pop_back();}return false;
  };
  if(!choose(0)){status="no_nontrivial_binary_compatible_prefix";return;}
  std::vector<I> guard=fixedx;guard.insert(guard.end(),fixedy.begin(),fixedy.end());
  const size_t nn=2ULL*variables;std::vector<unsigned char> base_seen(nn,0);std::vector<Reason> base_reason(nn);
  auto propagate=[&](std::vector<unsigned char>&seen,std::vector<Reason>&why,const std::vector<I>&roots,bool use_base,std::vector<U>&conflict){
   std::vector<I> todo;I failed=0;Reason attempted;
   auto add=[&](I v,Reason r){charge();if(seen[node(-v)]){failed=v;attempted=r;return false;}if(!seen[node(v)]){seen[node(v)]=1;why[node(v)]=r;todo.push_back(v);}return true;};
   for(I root:roots)if(!add(root,{}))break;
   while(!failed&&!todo.empty()){I from=todo.back();todo.pop_back();for(U k=offsets[node(from)];k<offsets[node(from)+1];++k){charge();auto e=edges[k];if(!add(e.to,{from,e.id}))break;}}
   if(!failed)return true;
   std::set<I> visited;std::vector<I> trace{-failed};if(attempted.id){conflict.push_back(attempted.id);trace.push_back(attempted.from);}
   while(!trace.empty()){I v=trace.back();trace.pop_back();charge();if(!visited.insert(v).second)continue;Reason r=use_base&&base_seen[node(v)]?base_reason[node(v)]:why[node(v)];if(r.id){conflict.push_back(r.id);trace.push_back(r.from);}if(conflict.size()>1024)throw Stop("conflict_witness_bound");}
   std::sort(conflict.begin(),conflict.end());conflict.erase(std::unique(conflict.begin(),conflict.end()),conflict.end());return false;
  };
  std::vector<U> base_conflict;if(!propagate(base_seen,base_reason,guard,false,base_conflict)){status="prefix_binary_conflict";return;}
  certificate_masks.assign(nn,0);std::vector<bool> viable(24,false);std::vector<std::string> cases;
  auto ilist=[](const std::vector<I>&v){std::ostringstream o;o<<'[';for(size_t i=0;i<v.size();++i){if(i)o<<',';o<<v[i];}return o.str()+']';};
  auto certify=[&](unsigned bit,const std::vector<I>&extra){auto seen=base_seen;std::vector<Reason> why(nn);std::vector<U> conflict;bool ok=propagate(seen,why,extra,true,conflict);viable[bit]=ok;if(ok)for(size_t n=0;n<nn;++n){if(!(n&4095))wall();if(seen[n])certificate_masks[n]|=U(1)<<bit;}evidence_ids.insert(evidence_ids.end(),conflict.begin(),conflict.end());std::vector<I> assumptions=guard;assumptions.insert(assumptions.end(),extra.begin(),extra.end());std::ostringstream o;o<<"{\"bit\":"<<bit<<",\"assumptions\":"<<ilist(assumptions)<<",\"unexcluded\":"<<(ok?"true":"false")<<",\"binary_conflict_premises\":"<<list(conflict)<<'}';cases.push_back(o.str());};
  auto&ux=domains[unauthorized.back()].v;auto&ay=domains[authorized.back()].v;
  for(unsigned i=0;i<4;++i){certify(i,{ux[i]});certify(4+i,{ay[i]});}
  for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)certify(8+4*i+j,{ux[i],ay[j]});
  std::vector<unsigned> remaining_x(4,0),remaining_y(4,0);for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)if(viable[8+4*i+j]&&partner[ux[i]]!=ay[j]){++remaining_x[i];++remaining_y[j];}
  bool nontrivial=true;unsigned live_x=0,live_y=0;for(unsigned i=0;i<4;++i){if(viable[i]){++live_x;nontrivial&=remaining_x[i]>=2;}if(viable[4+i]){++live_y;nontrivial&=remaining_y[i]>=2;}}nontrivial&=live_x>=3&&live_y>=3;
  std::ostringstream support;support<<"{\"prefix_unauthorized\":"<<ilist(fixedx)<<",\"prefix_authorized\":"<<ilist(fixedy)<<",\"residual_unauthorized\":"<<ilist(ux)<<",\"residual_authorized\":"<<ilist(ay)<<",\"nontrivial\":"<<(nontrivial?"true":"false")<<",\"remaining_supports_unauthorized\":[";for(unsigned i=0;i<4;++i){if(i)support<<',';support<<remaining_x[i];}support<<"],\"remaining_supports_authorized\":[";for(unsigned i=0;i<4;++i){if(i)support<<',';support<<remaining_y[i];}support<<"],\"cases\":[";for(size_t i=0;i<cases.size();++i){if(i)support<<',';support<<cases[i];}support<<"]}";support_description=support.str();
  for(unsigned ri=0;ri<4;++ri){I residual=ux[ri];std::vector<I> x=fixedx,y=fixedy,candidate;x.push_back(residual);y.push_back(partner[residual]);std::vector<U> ids{cover.id};for(I v:x)candidate.push_back(-v);for(I v:y)candidate.push_back(-v);
   for(size_t p=0;p<8;++p)for(I value:domains[unauthorized[p]].v){auto&b=blocks[rows[unauthorized[p]][value]];ids.push_back(b.xid);ids.push_back(b.qid);if(value!=x[p]){U amo=find(-x[p],-value);if(!amo){status="missing_amo";return;}ids.push_back(amo);}else {U forward=find(-y[p],b.q);if(!forward){status="missing_authorized_forward";return;}ids.push_back(forward);}}
   std::sort(candidate.begin(),candidate.end());U duplicate=existing(candidate);unsigned rj=unsigned(std::find(ay.begin(),ay.end(),partner[residual])-ay.begin());bool implied=!viable[8+4*ri+rj];std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());evidence_ids.insert(evidence_ids.end(),ids.begin(),ids.end());
   std::ostringstream out;out<<"{\"clause\":"<<ilist(candidate)<<",\"residual_values\":["<<residual<<','<<partner[residual]<<"],\"original_premises\":"<<list(ids)<<",\"existing_occurrence\":"<<duplicate<<",\"binary_implied\":"<<(implied?"true":"false")<<",\"novel\":"<<(!duplicate&&!implied?"true":"false")<<",\"residual_unexcluded_supports\":["<<remaining_x[ri]<<','<<remaining_y[rj]<<"],\"proof_validation\":\"not_run\"}";offers.push_back(out.str());
  }
  query_complete=true;status="four_aligned_tuples_require_independent_original_cnf_rup";
 }
 void output(const std::string&path){std::ofstream certificate(path+".binary-certificate");for(size_t n=0;n<certificate_masks.size();++n)if(certificate_masks[n]){I lit=I(n/2+1);if(n&1)lit=-lit;certificate<<lit<<' '<<certificate_masks[n]<<'\n';}
 std::ofstream o(path);o<<"{\"schema\":\"blocker-support-admission-v1\",\"status\":\""<<status<<"\",\"source_complete\":"<<(complete?"true":"false")<<",\"query_complete\":"<<(query_complete?"true":"false")<<",\"input_hash_complete\":"<<(hash_complete?"true":"false")<<",\"input_file_sha256\":"<<(hash_complete?'"'+hash+'"':"null")<<",\"compressed_bytes\":"<<compressed<<",\"decoded_bytes\":"<<bytes<<",\"clauses_scanned\":"<<occurrences<<",\"sort_work\":"<<sort_work<<",\"sort_work_cap\":"<<sort_cap<<",\"metadata_bytes_charged\":"<<metadata_bytes<<",\"binary_count\":"<<binary.size()<<",\"query_work\":"<<query_work<<",\"query_cap\":"<<query_cap<<",\"seed\":"<<seed<<",\"selected_domains\":[";
  for(size_t j=0;j<selected.size();++j){if(j)o<<',';auto&d=domains[selected[j]];o<<"{\"alo\":"<<d.id<<",\"values\":[";for(size_t i=0;i<d.v.size();++i){if(i)o<<',';o<<d.v[i];}o<<"],\"amo\":[";bool first=true;for(size_t a=0;a<d.v.size();++a)for(size_t b=a+1;b<d.v.size();++b){I x=-d.v[a],y=-d.v[b];order(x,y);auto it=std::lower_bound(binary.begin(),binary.end(),Binary{x,y,0},[](auto l,auto r){return std::tie(l.a,l.b,l.id)<std::tie(r.a,r.b,r.id);});if(!first)o<<',';first=false;o<<(it!=binary.end()&&it->a==x&&it->b==y?it->id:0);}o<<"]}";}
  o<<"],\"support_analysis\":"<<support_description<<",\"blocker_cover\":"<<cover_description<<",\"authenticated_or_definitions\":[";for(size_t j=0;j<authenticated_defs.size();++j){if(j)o<<',';auto&d=authenticated_defs[j];I q=-d.v[0];o<<"{\"output\":"<<q<<",\"reverse\":"<<d.id<<",\"inputs\":[";for(size_t i=1;i<d.v.size();++i){if(i>1)o<<',';o<<d.v[i];}o<<"],\"forward\":[";for(size_t i=1;i<d.v.size();++i){if(i>1)o<<',';I a=-d.v[i],b=q;order(a,b);auto it=std::lower_bound(binary.begin(),binary.end(),Binary{a,b,0},[](auto l,auto r){return std::tie(l.a,l.b,l.id)<std::tie(r.a,r.b,r.id);});o<<(it!=binary.end()&&it->a==a&&it->b==b?it->id:0);}o<<"]}";}o<<"],\"outgoing_neighbors\":[";for(size_t i=0;i<outgoing.size();++i){if(i)o<<',';o<<"{\"signed_literal\":"<<outgoing[i].to<<",\"premise\":"<<outgoing[i].id<<'}';}o<<"],\"helpers\":[";for(size_t i=0;i<helpers.size();++i){if(i)o<<',';auto&h=helpers[i];o<<"{\"clause\":["<<h.a<<','<<h.b<<"],\"original_premises\":"<<list(h.ids)<<'}';}o<<"],\"aligned_tuples\":[";for(size_t i=0;i<offers.size();++i){if(i)o<<',';o<<offers[i];}o<<"],\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"proof_validation\":\"not_run\",\"solver_run\":false}\n";
 }
};
int main(int argc,char**argv){if(argc!=14){std::cerr<<"input output sha variables clauses decoded_cap seed seconds query_cap memory_bytes compressed_cap seed_blocker authorized_domain_begin\n";return 2;}Tool t;std::string out=argv[2];int rc=0;try{t.seed=std::stoi(argv[7]);t.seed_blocker=std::stoi(argv[12]);t.authorized_domain_begin=U(std::stoul(argv[13]));t.seconds=std::stod(argv[8]);t.query_cap=std::stoull(argv[9]);uint64_t mem=std::stoull(argv[10]);if(t.seed_blocker<=0||t.seed<=0||t.seconds<=0||t.seconds>30||!t.query_cap||mem>256ULL*1024*1024||mem<16ULL*1024*1024)throw std::runtime_error("invalid_bounds");rlimit limit{mem,mem};if(setrlimit(RLIMIT_AS,&limit))throw std::runtime_error("memory_guard");t.parse(argv[1],argv[3],U(std::stoul(argv[4])),U(std::stoul(argv[5])),std::stoull(argv[6]),std::stoull(argv[11]));if(U(t.seed)>t.variables)throw std::runtime_error("seed_header");t.index();t.query();}catch(const Stop&e){t.status=e.what();}catch(const std::bad_alloc&){t.status="allocation_bound";}catch(const std::exception&e){t.status=e.what();rc=2;}t.output(out);return rc;}
