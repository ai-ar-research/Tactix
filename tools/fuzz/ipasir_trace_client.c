/* Stateful trace client. Deliberately uses only public C headers. */
#undef NDEBUG
#include "tactix_ipasir.h"
#include "tactix.h"
#include "public_random_session.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct client {
  unsigned id;
  void *solver;
  unsigned variables;
  int *assumptions;
  size_t size;
  int termination_limit;
  unsigned termination_calls;
  int strategy, learning_limit, learning_budget, core_budget;
  int min_budget,min_context,min_stop;
  unsigned strategy_calls, restored, learned_count;
  int **learned;
  unsigned *learned_sizes;
  uint64_t random;
  tactix_random_session *random_policy;

  struct client *next;
} client;

static const char *native_driver(int strategy) {
  return strategy==4?"cruxmiter":strategy==6?"dpll":strategy==7?"cdcl-no-restarts":
      strategy==8?"scripted-default":NULL;
}

typedef struct retained_result {
  tactix_ipasir_result *result;
  unsigned index;
  int status;
  size_t variables,core_size,assumptions_size;
  int *values,*core,*assumptions;
  struct retained_result *next;
} retained_result;

static int terminate(void *data) {
  client *c=data;++c->termination_calls;
  return c->termination_limit>0&&c->termination_calls>=(unsigned)c->termination_limit;
}

static void clear_learned(client *c) {
  for(unsigned i=0;i<c->learned_count;++i)free(c->learned[i]);
  free(c->learned);free(c->learned_sizes);
  c->learned=NULL;c->learned_sizes=NULL;c->learned_count=0;
}
static void learn(void *data,int *body) {
  client *c=data;unsigned size=0;
  while(body[size]) {assert(c->learning_limit>=0&&size<(unsigned)c->learning_limit);++size;}
  assert(c->learned_count<UINT_MAX);
  int **grown=realloc(c->learned,(c->learned_count+1)*sizeof *grown);assert(grown);c->learned=grown;
  unsigned *sizes=realloc(c->learned_sizes,(c->learned_count+1)*sizeof *sizes);assert(sizes);c->learned_sizes=sizes;
  int *copy=malloc((size+1)*sizeof *copy);assert(copy);memcpy(copy,body,(size+1)*sizeof *copy);
  c->learned[c->learned_count]=copy;c->learned_sizes[c->learned_count++]=size;
  body[0]=INT_MAX; // Scratch ownership is part of the public callback contract.
}
static int rule_id(const char *name) {
  for(int i=0;i<tactix_rule_count();++i)if(!strcmp(tactix_rule_name(i),name))return i;
  assert(0);return -1;
}
static int scripted(void *data,void *session,tactix *view,unsigned entry) {
  client *c=data;++c->strategy_calls;c->restored+=entry==TACTIX_IPASIR_ENTER_RESTORED;
  if(c->strategy==5)return tactix_random_session_run(c->random_policy,session,view,entry);
  if(c->strategy==3) {
    assert(!tactix_set_default_option(view,"TACTIX_LUCKY_LATE","0"));
    return tactix_solve(view); // Explicit configured handoff, never an implicit fallback.
  }
  for(;;) {
    tactix_ipasir_query_state q={0};q.version=1;q.size=sizeof q;assert(!tactix_ipasir_next(session,&q));
    switch(q.action) {
    case TACTIX_QUERY_ASSUME: {
      const int code=tactix_ipasir_assume_next(session);
      if(code==TACTIX_IPASIR_ACTION_INTERRUPTED)return 0;
      assert(!code);break;
    }
    case TACTIX_QUERY_PROPAGATE:assert(tactix_apply(view,rule_id("Propagate"))>=0);break;
    case TACTIX_QUERY_ANALYZE:assert(tactix_apply_instance(view,rule_id("ConflictAnalysis"),0)>=0);break;
    case TACTIX_QUERY_BACKTRACK:assert(tactix_apply_instance(view,rule_id("Restart"),0)>=0);break;
    case TACTIX_QUERY_READY: {
      int candidates[64];const int count=tactix_decide_candidates(view,candidates,64);assert(count>0);
      int literal=candidates[0];
      if(c->strategy==2) {c->random^=c->random<<13;c->random^=c->random>>7;c->random^=c->random<<17;literal=candidates[c->random%(unsigned)count];if(c->random&64)literal=-literal;}
      assert(!tactix_decide(view,literal));break;
    }
    case TACTIX_QUERY_SAT:case TACTIX_QUERY_UNSAT:return q.action;
    case TACTIX_QUERY_RESTORE:case TACTIX_QUERY_INTERRUPTED:return 0;
    default:assert(0);
    }
  }
}

static client *find(client *head,unsigned id) {
  while(head&&head->id!=id)head=head->next;
  return head;
}
static void copy_member(const char *prefix,const char *bundle,const char *suffix,unsigned index) {
  char source[4096],target[4096];
  assert(snprintf(source,sizeof source,"%s%s",bundle,suffix)>0);
  assert(snprintf(target,sizeof target,"%s-q%u%s",prefix,index,suffix)>0);
  FILE *in=fopen(source,"rb"),*out=fopen(target,"wb");assert(in&&out);
  char data[4096];size_t n;
  while((n=fread(data,1,sizeof data,in)))assert(fwrite(data,1,n,out)==n);
  assert(!ferror(in)&&!fclose(in)&&!fclose(out));
}
static void compare_member(const char *prefix,const char *bundle,const char *suffix,unsigned index) {
  char source[4096],target[4096];
  snprintf(source,sizeof source,"%s%s",bundle,suffix);
  snprintf(target,sizeof target,"%s-q%u%s",prefix,index,suffix);
  FILE *in=fopen(source,"rb"),*saved=fopen(target,"rb");
  assert((in!=NULL)==(saved!=NULL));if(!in)return;
  for(;;) {
    unsigned char a[4096],b[4096];size_t n=fread(a,1,sizeof a,in),m=fread(b,1,sizeof b,saved);
    assert(n==m&&!memcmp(a,b,n));if(n<sizeof a)break;
  }
  assert(!ferror(in)&&!ferror(saved)&&!fclose(in)&&!fclose(saved));
}
static retained_result *remember_handle(tactix_ipasir_result *handle,unsigned index,int status) {
  retained_result *r=calloc(1,sizeof *r);assert(r);
  r->result=handle;assert(r->result);
  r->index=index;r->status=status;
  assert(tactix_ipasir_result_status(r->result)==status);
  r->variables=tactix_ipasir_result_variables(r->result);
  r->core_size=tactix_ipasir_result_core_size(r->result);
  r->assumptions_size=tactix_ipasir_result_assumptions(handle);
  r->values=calloc(r->variables+1,sizeof(int));r->core=calloc(r->core_size+1,sizeof(int));
  r->assumptions=calloc(r->assumptions_size+1,sizeof(int));assert(r->values&&r->core&&r->assumptions);
  for(size_t i=0;i<r->assumptions_size;++i)
    r->assumptions[i]=tactix_ipasir_result_assumption(handle,i);
  for(size_t i=0;i<r->variables;++i) {
    r->values[i]=tactix_ipasir_result_val(handle,(int)i+1);
  }
  for(size_t i=0;i<r->core_size;++i) {
    r->core[i]=tactix_ipasir_result_core_literal(r->result,i);
    assert(tactix_ipasir_result_failed(handle,r->core[i]));
  }
  return r;
}
static retained_result *remember(client *c,unsigned index,int status) {
  retained_result *r=remember_handle(tactix_ipasir_result_acquire(c->solver),index,status);
  assert(r->assumptions_size==c->size);
  for(size_t i=0;i<c->size;++i)assert(r->assumptions[i]==c->assumptions[i]&&
      tactix_ipasir_result_failed(r->result,c->assumptions[i])==ipasir_failed(c->solver,c->assumptions[i]));
  for(size_t i=0;i<r->variables;++i)assert(r->values[i]==ipasir_val(c->solver,(int)i+1));
  return r;
}
static void export_saved(const char *prefix,const retained_result *r) {
  const char *bundle=tactix_ipasir_result_bundle(r->result);
  const char *suffixes[]={".cnf",".proof",".json",r->status==10?".model":".core"};
  for(unsigned i=0;i<4;++i)copy_member(prefix,bundle,suffixes[i],r->index);
  char reduced[4096];snprintf(reduced,sizeof reduced,"%s-core.cnf",bundle);
  FILE *core=fopen(reduced,"rb");
  if(core) {assert(!fclose(core));copy_member(prefix,bundle,"-core.cnf",r->index);copy_member(prefix,bundle,"-core.proof",r->index);}
}
static void print_vector(const int *values,size_t size) {
  putchar('[');const char *separator="";
  for(size_t i=0;i<size;++i)if(values[i]) {printf("%s%d",separator,values[i]);separator=",";}
  putchar(']');
}
static void print_minimization(client *c,const char *prefix,retained_result *source,
    retained_result **results,unsigned *artifact) {
  if(!source || source->status!=20 || c->min_budget==-2) {printf("null");return;}
  int context[31];size_t context_size=0;
  for(size_t i=0;i<source->assumptions_size&&i<31;++i)
    if((unsigned)c->min_context&(1u<<i))context[context_size++]=source->assumptions[i];
  const char *before=tactix_ipasir_bundle(c->solver);assert(before);
  const unsigned calls=c->strategy_calls,restored=c->restored,learned=c->learned_count;
  const int saved_limit=c->termination_limit;
  const unsigned saved_polls=c->termination_calls;
  if(c->min_stop!=-2) {
    c->termination_limit=c->min_stop;c->termination_calls=0;
    ipasir_set_terminate(c->solver,c,c->min_stop<0?NULL:terminate);
  }
  tactix_ipasir_strategy_counters a={0},b={0};a.version=b.version=1;a.size=b.size=sizeof a;
  if(native_driver(c->strategy))assert(!tactix_ipasir_get_strategy_counters(c->solver,&a));
  const uint64_t budget=c->min_budget<0?UINT64_MAX:(unsigned)c->min_budget;
  tactix_ipasir_reduction *r=tactix_ipasir_minimize_core(c->solver,source->result,context,context_size,budget);assert(r);
  const unsigned min_polls=c->termination_calls;
  const int min_limit=c->termination_limit;
  if(c->min_stop!=-2) {
    c->termination_limit=saved_limit;c->termination_calls=saved_polls;
    ipasir_set_terminate(c->solver,c,saved_limit<0?NULL:terminate);
  }
  tactix_ipasir_result *original=tactix_ipasir_reduction_source(r);assert(original);
  assert(tactix_ipasir_result_status(original)==20&&tactix_ipasir_result_assumptions(original)==source->assumptions_size);
  assert(!strcmp(tactix_ipasir_result_bundle(original),tactix_ipasir_result_bundle(source->result)));
  assert(!strcmp(tactix_ipasir_result_input_sha256(original),tactix_ipasir_result_input_sha256(source->result)));
  for(size_t i=0;i<source->assumptions_size;++i)
    assert(tactix_ipasir_result_assumption(original,i)==source->assumptions[i]);
  tactix_ipasir_result_release(original);
  assert(before==tactix_ipasir_bundle(c->solver));
  tactix_ipasir_result *current=tactix_ipasir_result_acquire(c->solver);assert(current);
  assert(tactix_ipasir_result_status(current)==20&&tactix_ipasir_result_assumptions(current)==source->assumptions_size);
  assert(!strcmp(tactix_ipasir_result_bundle(current),tactix_ipasir_result_bundle(source->result)));
  for(size_t i=0;i<source->assumptions_size;++i)assert(
      tactix_ipasir_result_assumption(current,i)==source->assumptions[i]&&
      ipasir_failed(c->solver,source->assumptions[i])==tactix_ipasir_result_failed(source->result,source->assumptions[i]));
  tactix_ipasir_result_release(current);
  unsigned entered=c->strategy_calls-calls,resumed=c->restored-restored;
  if(native_driver(c->strategy)) {
    assert(!tactix_ipasir_get_strategy_counters(c->solver,&b));
    entered=(unsigned)(b.query_entries-a.query_entries+b.restored_entries-a.restored_entries);
    resumed=(unsigned)(b.restored_entries-a.restored_entries);
  }
  tactix_ipasir_result *cert=tactix_ipasir_reduction_certificate(r);assert(cert);
  const char *cert_bundle=tactix_ipasir_result_bundle(cert);unsigned certificate=source->index;
  int matched=!strcmp(cert_bundle,tactix_ipasir_result_bundle(source->result));
  printf("{\"minimal\":%s,\"source\":%u,\"strategy_calls\":%u,\"restorations\":%u,\"termination_limit\":%d,\"termination_calls\":%u,\"context\":[",
      tactix_ipasir_reduction_minimal(r)?"true":"false",source->index,entered,resumed,min_limit,min_polls);
  for(size_t i=0;i<tactix_ipasir_reduction_context_size(r);++i)
    printf("%s%d",i?",":"",tactix_ipasir_reduction_context_literal(r,i));
  printf("],\"core\":[");
  for(size_t i=0;i<tactix_ipasir_reduction_core_size(r);++i)
    printf("%s%d",i?",":"",tactix_ipasir_reduction_core_literal(r,i));
  printf("],\"probes\":[");
  for(size_t i=0;i<tactix_ipasir_reduction_probes(r);++i) {
    const int status=tactix_ipasir_reduction_probe_status(r,i);
    assert(*artifact<UINT_MAX);const unsigned index=++*artifact;
    printf("%s{\"result\":%d,\"bundle_index\":%u,\"owned_result\":%d,\"assumptions\":[",i?",":"",status,index,status!=0);
    for(size_t j=0;j<tactix_ipasir_reduction_probe_assumptions(r,i);++j)
      printf("%s%d",j?",":"",tactix_ipasir_reduction_probe_assumption(r,i,j));
    retained_result *saved=NULL;
    if(status) {
      saved=remember_handle(tactix_ipasir_reduction_probe_result(r,i),index,status);
      export_saved(prefix,saved);saved->next=*results;*results=saved;
      if(!strcmp(cert_bundle,tactix_ipasir_result_bundle(saved->result))) {certificate=index;matched=1;}
    } else assert(!tactix_ipasir_reduction_probe_result(r,i));
    printf("],\"values\":");print_vector(saved?saved->values:NULL,saved?saved->variables:0);
    printf(",\"failed\":");print_vector(saved?saved->core:NULL,saved?saved->core_size:0);putchar('}');
  }
  assert(matched);printf("],\"certificate\":%u,\"witnesses\":[",certificate);
  for(size_t i=0;i<tactix_ipasir_reduction_witnesses(r);++i)
    printf("%s[%d,%zu]",i?",":"",tactix_ipasir_reduction_witness_literal(r,i),tactix_ipasir_reduction_witness_probe(r,i));
  printf("],\"learned\":[");
  for(unsigned i=learned;i<c->learned_count;++i) {printf("%s[",i==learned?"":",");for(unsigned j=0;j<c->learned_sizes[i];++j)printf("%s%d",j?",":"",c->learned[i][j]);putchar(']');}
  printf("]}");tactix_ipasir_result_release(cert);tactix_ipasir_reduction_release(r);
}
int main(int argc,char **argv) {
  assert(argc==3);
  FILE *trace=fopen(argv[1],"r");assert(trace);
  client *head=NULL;retained_result *results=NULL;unsigned query=0,artifact=0;
  char line[256],op[24],extra;unsigned id;int lit;
  while(fgets(line,sizeof line,trace)) {
    if(line[0]=='#'||line[0]=='\n')continue;
    assert(strchr(line,'\n')||feof(trace));
    const int words=sscanf(line,"%23s %u %d %c",op,&id,&lit,&extra);
    assert(words==2||words==3);
    client *c=find(head,id);
    if(!strcmp(op,"init")) {
      assert(words==2&&(!c||!c->solver));
      if(!c) {c=calloc(1,sizeof *c);assert(c);c->id=id;c->next=head;head=c;}
      clear_learned(c);
      c->solver=ipasir_init();assert(c->solver);c->variables=0;c->size=0;c->termination_limit=-1;c->termination_calls=0;
      assert(!tactix_ipasir_result_acquire(c->solver));
      c->strategy=0;c->learning_limit=-1;c->learning_budget=-1;c->core_budget=0;c->strategy_calls=c->restored=0;
      c->min_budget=-2;c->min_context=0;c->min_stop=-2;
      c->random=UINT64_C(0x9e3779b97f4a7c15)^(uint64_t)id;
      c->random_policy=tactix_random_session_new(c->random);assert(c->random_policy);
    } else {
      assert(c&&c->solver);
      if(!strcmp(op,"release")) {
        assert(words==2);ipasir_release(c->solver);c->solver=NULL;c->size=0;clear_learned(c);
        tactix_random_session_delete(c->random_policy);c->random_policy=NULL;
      } else if(!strcmp(op,"terminate")) {
        assert(words==3&&lit>=-1);
        const char *before=tactix_ipasir_bundle(c->solver);
        c->termination_limit=lit;
        ipasir_set_terminate(c->solver,c,lit<0?NULL:terminate);
        assert(before==tactix_ipasir_bundle(c->solver));
      } else if(!strcmp(op,"random_seed")) {
        assert(words==3&&lit>=0);
        tactix_random_session_seed(c->random_policy,(uint64_t)lit);
      } else if(!strcmp(op,"min_budget")||!strcmp(op,"min_context")||!strcmp(op,"min_stop")) {
        assert(words==3);
        if(!strcmp(op,"min_budget")) {assert(lit>=-2);c->min_budget=lit;}
        else if(!strcmp(op,"min_stop")) {assert(lit>=-2);c->min_stop=lit;}
        else {assert(lit>=0);c->min_context=lit;}
      } else if(!strcmp(op,"strategy")||!strcmp(op,"learn")||!strcmp(op,"learn_budget")||!strcmp(op,"core_budget")) {
        assert(words==3);const char *before=tactix_ipasir_bundle(c->solver);
        if(!strcmp(op,"strategy")) {
          assert(lit>=0&&lit<=8);c->strategy=lit;
          if(native_driver(lit))assert(!tactix_ipasir_set_strategy_name(c->solver,native_driver(lit)));
          else assert(!tactix_ipasir_set_strategy(c->solver,c,lit?scripted:NULL));
        } else if(!strcmp(op,"learn")) {
          assert(lit>=-1);c->learning_limit=lit;ipasir_set_learn(c->solver,c,lit,lit<0?NULL:learn);
        } else {
          assert(lit>=-1);const uint64_t ticks=lit<0?UINT64_MAX:(unsigned)lit;
          if(!strcmp(op,"core_budget")) {c->core_budget=lit;assert(!tactix_ipasir_set_core_budget(c->solver,ticks));}
          else {c->learning_budget=lit;assert(!tactix_ipasir_set_learn_budget(c->solver,ticks));}
        }
        assert(before==tactix_ipasir_bundle(c->solver));
      } else if(!strcmp(op,"add")||!strcmp(op,"assume")) {
        assert(words==3&&lit!=INT_MIN);
        const unsigned variable=lit<0?(unsigned)-lit:(unsigned)lit;
        if(variable>c->variables)c->variables=variable;
        if(!strcmp(op,"add"))ipasir_add(c->solver,lit);
        else {
          assert(lit);
          int *grown=realloc(c->assumptions,(c->size+1)*sizeof *grown);assert(grown);
          c->assumptions=grown;c->assumptions[c->size++]=lit;
          ipasir_assume(c->solver,lit);
        }
        assert(!tactix_ipasir_bundle(c->solver));
        assert(!tactix_ipasir_result_acquire(c->solver));
      } else {
        assert(!strcmp(op,"solve")&&words==2);
        clear_learned(c);c->termination_calls=0;c->strategy_calls=c->restored=0;
        tactix_random_session_variables(c->random_policy,c->variables);
        tactix_ipasir_strategy_counters before={0},after={0};
        before.version=after.version=1;before.size=after.size=sizeof before;
        if(native_driver(c->strategy))assert(!tactix_ipasir_get_strategy_counters(c->solver,&before));
        const int result=ipasir_solve(c->solver);
        if(native_driver(c->strategy)) {
          assert(!tactix_ipasir_get_strategy_counters(c->solver,&after));
          c->strategy_calls=(unsigned)(after.query_entries-before.query_entries+after.restored_entries-before.restored_entries);
          c->restored=(unsigned)(after.restored_entries-before.restored_entries);
        }
        assert(result==0||result==10||result==20);
        const char *bundle=tactix_ipasir_bundle(c->solver);
        if(!result)assert(!bundle&&!tactix_ipasir_result_acquire(c->solver)&&c->termination_limit>0&&c->termination_calls>=(unsigned)c->termination_limit);
        else assert(bundle);
        ++query;assert(artifact<UINT_MAX);++artifact;
        retained_result *source=NULL;
        if(result) {
          source=remember(c,artifact,result);source->next=results;results=source;export_saved(argv[2],source);
        }
        printf("{\"index\":%u,\"bundle_index\":%u,\"session\":%u,\"result\":%d,\"owned_result\":%d,\"termination_limit\":%d,\"termination_calls\":%u,\"values\":[",query,artifact,id,result,result!=0,c->termination_limit,c->termination_calls);
        const char *separator="";
        if(result==10)for(unsigned v=1;v<=c->variables;++v) {
          int value=ipasir_val(c->solver,(int)v);
          assert(value==ipasir_val(c->solver,-(int)v));
          assert(!value||value==(int)v||value==-(int)v);
          if(value) {printf("%s%d",separator,value);separator=",";}
        }
        printf("],\"failed\":[");separator="";
        if(result==20)for(size_t i=0;i<c->size;++i) {
          if(ipasir_failed(c->solver,c->assumptions[i])) {
            printf("%s%d",separator,c->assumptions[i]);separator=",";
          }
        }
        printf("],\"strategy\":%d,\"strategy_calls\":%u,\"restorations\":%u,\"learning_limit\":%d,\"learning_budget\":%d,\"core_budget\":%d,\"learned\":[",c->strategy,c->strategy_calls,c->restored,c->learning_limit,c->learning_budget,c->core_budget);
        for(unsigned i=0;i<c->learned_count;++i) {printf("%s[",i?",":"");for(unsigned j=0;j<c->learned_sizes[i];++j)printf("%s%d",j?",":"",c->learned[i][j]);putchar(']');}
        printf("],\"random_policy\":");
        if(c->strategy==5)tactix_random_session_coverage(c->random_policy,stdout);
        else printf("null");
        printf(",\"driver_name\":\"%s\",\"native_root_requests\":%llu,\"native_root_eligible\":%llu,\"native_root_effective\":%llu,\"native_root_rounds\":%llu,\"native_handoffs\":%llu",
            tactix_ipasir_strategy_name(c->solver),
            (unsigned long long)(after.root_requests-before.root_requests),
            (unsigned long long)(after.root_eligible-before.root_eligible),
            (unsigned long long)(after.root_effective-before.root_effective),
            (unsigned long long)(after.root_rounds-before.root_rounds),
            (unsigned long long)(after.default_handoffs-before.default_handoffs));
        printf(",\"min_budget\":%d,\"min_context\":%d,\"min_stop\":%d,\"minimization\":",c->min_budget,c->min_context,c->min_stop);
        print_minimization(c,argv[2],source,&results,&artifact);printf("}\n");
        fflush(stdout);c->size=0;
      }
    }
  }
  assert(!ferror(trace)&&!fclose(trace));
  while(head) {client *next=head->next;if(head->solver)ipasir_release(head->solver);tactix_random_session_delete(head->random_policy);clear_learned(head);free(head->assumptions);free(head);head=next;}
  unsigned checked=0;
  while(results) {
    retained_result *r=results;results=r->next;
    assert(tactix_ipasir_result_status(r->result)==r->status);
    assert(tactix_ipasir_result_variables(r->result)==r->variables);
    assert(tactix_ipasir_result_core_size(r->result)==r->core_size);
    assert(tactix_ipasir_result_assumptions(r->result)==r->assumptions_size);
    for(size_t i=0;i<r->variables;++i)assert(tactix_ipasir_result_val(r->result,(int)i+1)==r->values[i]);
    for(size_t i=0;i<r->core_size;++i)assert(tactix_ipasir_result_core_literal(r->result,i)==r->core[i]&&tactix_ipasir_result_failed(r->result,r->core[i]));
    for(size_t i=0;i<r->assumptions_size;++i)assert(tactix_ipasir_result_assumption(r->result,i)==r->assumptions[i]);
    const char *bundle=tactix_ipasir_result_bundle(r->result);assert(bundle);
    const char *suffixes[]={".json",".cnf",".proof",".model",".core","-core.cnf","-core.proof"};
    for(unsigned i=0;i<sizeof suffixes/sizeof *suffixes;++i)compare_member(argv[2],bundle,suffixes[i],r->index);
    tactix_ipasir_result_release(r->result);free(r->values);free(r->core);free(r->assumptions);free(r);++checked;
  }
  printf("c owned-results-checked %u\n",checked);
  return 0;
}
