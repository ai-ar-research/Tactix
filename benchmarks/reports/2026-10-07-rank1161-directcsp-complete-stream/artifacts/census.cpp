#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
 std::vector<Binary> binary;std::vector<Clause> domains,defs;
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
 void query(){const Clause*q=definition(seed);if(!q){status="not_admitted_missing_seed_definition";return;}authenticated_defs.push_back(*q);for(size_t i=1;i<q->v.size();++i)if(!add_domain(q->v[i])){status="not_admitted_missing_seed_domain";return;}
  auto append=[&](I x,I y,std::vector<U> ids){charge();if(x==y||owners[x]==owners[y]||std::any_of(ids.begin(),ids.end(),[](U id){return !id;})||find(-x,-y))return;I a=-x,b=-y;order(a,b);if(helper_keys.insert({a,b}).second)helpers.push_back({a,b,std::move(ids)});};
  // Only the seed's actual signed outgoing neighbors, never a lexical prefix.
  for(U k=offsets[node(seed)];k<offsets[node(seed)+1];++k){charge();auto e=edges[k];outgoing.push_back(e);I v=I(std::abs(int64_t(e.to)));
   if(U(v)<=keymax){if(!add_domain(v))throw Stop("missing_neighbor_domain");for(size_t i=1;i<q->v.size();++i){I x=q->v[i];if(e.to<0)append(x,v,{find(-x,seed),e.id});else for(I y:domains[owners[v]].v)if(y!=v)append(x,y,{find(-x,seed),e.id,find(-v,-y)});}continue;}
   const Clause*r=definition(v);if(!r)continue;authenticated_defs.push_back(*r);for(size_t j=1;j<r->v.size();++j)if(!add_domain(r->v[j]))throw Stop("missing_neighbor_domain");
   std::vector<I> candidates;if(e.to<0)candidates.assign(r->v.begin()+1,r->v.end());else for(auto owner:owners)candidates.push_back(owner.first);
   for(size_t i=1;i<q->v.size();++i)for(I y:candidates){charge();I x=q->v[i];std::vector<U> ids{find(-x,seed),e.id};
    if(e.to<0)ids.push_back(find(-y,v));else {bool incompatible=true;for(size_t h=1;h<r->v.size();++h)if(!find(-r->v[h],-y))incompatible=false;if(!incompatible)continue;ids.push_back(r->id);for(size_t h=1;h<r->v.size();++h)ids.push_back(find(-r->v[h],-y));}
    append(x,y,std::move(ids));
   }
  }
  // Exact ALO covering a union of incompatible values yields an original-variable RUP clause.
  std::map<std::pair<I,I>,std::vector<U>> forbidden;
  for(auto&b:binary){charge();if(b.a<0&&b.b<0&&owners.count(-b.a)&&owners.count(-b.b))forbidden[{-b.b,-b.a}]={b.id};}
  std::set<std::pair<I,I>> lifted;
  for(auto&h:helpers){charge();I a=-h.a,b=-h.b;if(a>b)std::swap(a,b);if(!forbidden.count({a,b})){forbidden[{a,b}]=h.ids;lifted.insert({a,b});}}
  auto key=[](I a,I b){if(a>b)std::swap(a,b);return std::make_pair(a,b);};
  for(size_t mid:selected){auto&d=domains[mid];for(auto ai=owners.begin();ai!=owners.end();++ai)for(auto ci=std::next(ai);ci!=owners.end();++ci){charge();I a=ai->first,c=ci->first;if(ai->second==mid||ci->second==mid||ai->second==ci->second)continue;unsigned sa=0,sc=0;bool covered=true,uses_lift=false;std::vector<U> witness{d.id};for(I b:d.v){auto ka=key(a,b),kc=key(c,b);bool fa=forbidden.count(ka),fc=forbidden.count(kc);sa+=!fa;sc+=!fc;if(!fa&&!fc){covered=false;break;}auto k=fa?ka:kc;uses_lift|=lifted.count(k);auto&ids=forbidden[k];witness.insert(witness.end(),ids.begin(),ids.end());}
    if(!covered||!uses_lift||sa<2||sc<2||find(-a,-c)||helper_keys.count({-c,-a})||reachable(a,-c)||reachable(c,-a))continue;
    std::ostringstream out;out<<"{\"clause\":["<<-c<<','<<-a<<"],\"middle_alo\":"<<d.id<<",\"supports_before\":["<<sa<<','<<sc<<"],\"original_premises\":"<<list(witness)<<",\"proof_validation\":\"not_run\"}";offers.push_back(out.str());if(offers.size()==16)throw Stop("offer_bound");
  }}query_complete=true;status=offers.empty()?"not_admitted":"candidate_requires_independent_proof_check";
 }
 void output(const std::string&path){std::ofstream o(path);o<<"{\"schema\":\"directcsp-complete-stream-v1\",\"status\":\""<<status<<"\",\"source_complete\":"<<(complete?"true":"false")<<",\"query_complete\":"<<(query_complete?"true":"false")<<",\"input_hash_complete\":"<<(hash_complete?"true":"false")<<",\"input_file_sha256\":"<<(hash_complete?'"'+hash+'"':"null")<<",\"compressed_bytes\":"<<compressed<<",\"decoded_bytes\":"<<bytes<<",\"clauses_scanned\":"<<occurrences<<",\"sort_work\":"<<sort_work<<",\"sort_work_cap\":"<<sort_cap<<",\"metadata_bytes_charged\":"<<metadata_bytes<<",\"binary_count\":"<<binary.size()<<",\"query_work\":"<<query_work<<",\"query_cap\":"<<query_cap<<",\"seed\":"<<seed<<",\"selected_domains\":[";
  for(size_t j=0;j<selected.size();++j){if(j)o<<',';auto&d=domains[selected[j]];o<<"{\"alo\":"<<d.id<<",\"values\":[";for(size_t i=0;i<d.v.size();++i){if(i)o<<',';o<<d.v[i];}o<<"],\"amo\":[";bool first=true;for(size_t a=0;a<d.v.size();++a)for(size_t b=a+1;b<d.v.size();++b){I x=-d.v[a],y=-d.v[b];order(x,y);auto it=std::lower_bound(binary.begin(),binary.end(),Binary{x,y,0},[](auto l,auto r){return std::tie(l.a,l.b,l.id)<std::tie(r.a,r.b,r.id);});if(!first)o<<',';first=false;o<<(it!=binary.end()&&it->a==x&&it->b==y?it->id:0);}o<<"]}";}
  o<<"],\"authenticated_or_definitions\":[";for(size_t j=0;j<authenticated_defs.size();++j){if(j)o<<',';auto&d=authenticated_defs[j];I q=-d.v[0];o<<"{\"output\":"<<q<<",\"reverse\":"<<d.id<<",\"inputs\":[";for(size_t i=1;i<d.v.size();++i){if(i>1)o<<',';o<<d.v[i];}o<<"],\"forward\":[";for(size_t i=1;i<d.v.size();++i){if(i>1)o<<',';I a=-d.v[i],b=q;order(a,b);auto it=std::lower_bound(binary.begin(),binary.end(),Binary{a,b,0},[](auto l,auto r){return std::tie(l.a,l.b,l.id)<std::tie(r.a,r.b,r.id);});o<<(it!=binary.end()&&it->a==a&&it->b==b?it->id:0);}o<<"]}";}o<<"],\"outgoing_neighbors\":[";for(size_t i=0;i<outgoing.size();++i){if(i)o<<',';o<<"{\"signed_literal\":"<<outgoing[i].to<<",\"premise\":"<<outgoing[i].id<<'}';}o<<"],\"helpers\":[";for(size_t i=0;i<helpers.size();++i){if(i)o<<',';auto&h=helpers[i];o<<"{\"clause\":["<<h.a<<','<<h.b<<"],\"original_premises\":"<<list(h.ids)<<'}';}o<<"],\"path_nogoods\":[";for(size_t i=0;i<offers.size();++i){if(i)o<<',';o<<offers[i];}o<<"],\"wall_seconds\":"<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<",\"proof_validation\":\"not_run\",\"solver_run\":false}\n";
 }
};
int main(int argc,char**argv){if(argc!=12){std::cerr<<"input output sha variables clauses decoded_cap seed seconds query_cap memory_bytes compressed_cap\n";return 2;}Tool t;std::string out=argv[2];int rc=0;try{t.seed=std::stoi(argv[7]);t.seconds=std::stod(argv[8]);t.query_cap=std::stoull(argv[9]);uint64_t mem=std::stoull(argv[10]);if(t.seed<=0||t.seconds<=0||t.seconds>30||!t.query_cap||mem>256ULL*1024*1024||mem<16ULL*1024*1024)throw std::runtime_error("invalid_bounds");rlimit limit{mem,mem};if(setrlimit(RLIMIT_AS,&limit))throw std::runtime_error("memory_guard");t.parse(argv[1],argv[3],U(std::stoul(argv[4])),U(std::stoul(argv[5])),std::stoull(argv[6]),std::stoull(argv[11]));if(U(t.seed)>t.variables)throw std::runtime_error("seed_header");t.index();t.query();}catch(const Stop&e){t.status=e.what();}catch(const std::bad_alloc&){t.status="allocation_bound";}catch(const std::exception&e){t.status=e.what();rc=2;}t.output(out);return rc;}
