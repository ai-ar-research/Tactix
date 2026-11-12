#include "tactix_ipasir.h"
#define tactix tactix_c_handle
#include "tactix.h"
#include "strategy/presets/cruxmiter.h"
#include "tactix_chvatal.h"
#include "tactix_search_demo.h"
#include "tactix_scripted_default.h"
#undef tactix
#include "api/api_internal.hpp"
#include "session/query_kernel.hpp"
#include "session/core_certificate.hpp"
#include "session/query_certificate.hpp"
#include "policies/default_policy.hpp"
#include "policies/strategy_settings.hpp"
#include "proof/proof.hpp"
#include "strategy/feedback_provenance.hpp"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unistd.h>

namespace {
using namespace tactix;
using namespace tactix::session;

NativeQueryLimits native_limits() {
  // These are representational bounds, not search-rule guards or tuned
  // thresholds. Storage grows on demand. Resource policy belongs to callers.
  constexpr auto u32=std::numeric_limits<uint32_t>::max();
  constexpr auto u64=std::numeric_limits<uint64_t>::max();
  constexpr auto size=std::numeric_limits<size_t>::max();
  return {{size,size,size},{u32,u64,u32,u64,u32,u32,u64},u64};
}

// Query owners retain only their own files; the last owner removes the store.
struct ArtifactStore {
  std::string directory;
  ~ArtifactStore() {
    if(!directory.empty()) {
      std::error_code error;
      std::filesystem::remove_all(directory,error);
    }
  }
};

struct QueryArtifacts {
  const std::shared_ptr<ArtifactStore> store;
  const std::string prefix;
  QueryArtifacts(std::shared_ptr<ArtifactStore> owner,std::string name)
      :store(std::move(owner)),prefix(std::move(name)) {}
  QueryArtifacts(const QueryArtifacts &)=delete;
  QueryArtifacts &operator=(const QueryArtifacts &)=delete;
  ~QueryArtifacts() {
    for(const char *suffix:{".cnf",".proof",".model",".core","-core.cnf","-core.proof",".json",".up-proof"}) {
      std::error_code error;
      std::filesystem::remove(prefix+suffix,error);
    }
  }
};

struct NativeRuntime {
  enum { scripted_default=3,chvatal=4 };
  cruxmiter_program program{};
  chvatal_program twins{};
  size_t twin_input_words=SIZE_MAX;
  tactix_search_demo search{};
  tactix_scripted_default scripted{};
  int demo_kind=0;
  StrategySettings options;
  uint64_t query_entries=0,restored_entries=0,handoffs=0;
  uint64_t requests=0,eligible=0,effective=0,rounds=0;
  NativeRuntime()=default;
  NativeRuntime(const NativeRuntime &)=delete;
  NativeRuntime &operator=(const NativeRuntime &)=delete;
  ~NativeRuntime() {chvatal_release(&twins);}
  bool begin(const StrategySettings &caller_options,const QueryHistory &history,
      QueryKernel &query,volatile sig_atomic_t *stopped) {
    if(demo_kind==scripted_default) {
      tactix_scripted_default_begin(&scripted);
      return true;
    } else if(demo_kind==chvatal) {
      requests+=twins.requests;eligible+=twins.eligible;
      effective+=twins.effective;rounds+=twins.completed_rounds;
      chvatal_reset(&twins);
      const auto input=history.input();
      if(twin_input_words!=input.size()) {
        std::vector<int> literals;std::vector<size_t> starts{0};int variables=0;
        for(int literal:input) {
          if(query.poll_terminate(stopped))return false;
          if(!literal)starts.push_back(literals.size());
          else {literals.push_back(literal);variables=std::max(variables,std::abs(literal));}
        }
        const chvatal_formula formula{literals.data(),starts.data(),starts.size()-1,literals.size(),variables};
        struct Stop {QueryKernel &query;volatile sig_atomic_t *stopped;} stop{query,stopped};
        if(!chvatal_begin(&twins,&formula,[](void *state) {
          auto &stop=*static_cast<Stop *>(state);return int(stop.query.poll_terminate(stop.stopped));
        },&stop))return false;
        twin_input_words=input.size();
      }
      return true;
    } else if(demo_kind) {
      return tactix_search_demo_begin(&search,demo_kind)!=0;
    } else {
      requests+=program.requests;eligible+=program.eligible;
      effective+=program.effective;rounds+=program.rounds;
      (void)cruxmiter_begin(&program);
    }
    options=caller_options;
    const char *value,*name;
    for(size_t i=0;(name=cruxmiter_setting(i,&value));++i)
      (void)api_detail::set_strategy_option(options,name,value);
    return true;
  }
};

struct PublishedResultView {
  std::shared_ptr<const QueryResultSnapshot> snapshot;
  std::vector<int32_t> assumptions;
  std::shared_ptr<QueryArtifacts> files;
  std::string input_hash;
};

struct Session {
  QueryHistory history;
  std::unique_ptr<QueryKernel> query=std::make_unique<QueryKernel>(native_limits());
  DefaultPolicy policy;
  StrategySettings options;
  void *execution=nullptr;
  void *strategy_state=nullptr;
  tactix_ipasir_strategy strategy=nullptr;
  std::unique_ptr<NativeRuntime> native;
  volatile sig_atomic_t stopped=0;
  bool in_strategy=false;
  std::shared_ptr<ArtifactStore> artifacts=std::make_shared<ArtifactStore>();
  std::shared_ptr<QueryArtifacts> current;
  // The public bundle contract retains published files until session release.
  std::vector<std::shared_ptr<QueryArtifacts>> retained;
  std::shared_ptr<const PublishedResultView> published;
  strategy::controller::FeedbackSha256 input_digest;
  size_t hashed_words=0;
  uint64_t attempt=0;
  uint64_t core_ticks=0;
  bool busy=false, input_error=false, minimizing=false;

  ~Session() {
    // Close the writer before removing any owned certificate files.
    clear_execution();query.reset();
  }

  bool initialize() {
    const char *tmp=std::getenv("TMPDIR");
    std::string pattern=std::string(tmp&&*tmp?tmp:"/tmp")+"/tactix-ipasir-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
    char *created=mkdtemp(name.data());
    if(!created)return false;
    artifacts->directory=created;
    constexpr char domain[]="tactix-permanent-input-i32le-v1\n";
    input_digest.append(domain,sizeof domain-1);return true;
  }

  void hash_input_suffix() {
    const auto input=history.input();
    for(;hashed_words<input.size();++hashed_words) {
      const auto word=uint32_t(input[hashed_words]);
      unsigned char bytes[4];
      for(unsigned i=0;i<4;++i)bytes[i]=static_cast<unsigned char>(word>>(8*i));
      input_digest.append(bytes,sizeof bytes);
    }
  }
  void clear_published() {
    published.reset();
  }

  std::unique_ptr<Proof> writer() {
    const auto *active=history.active_query();
    if(!active || attempt==UINT64_MAX)return {};
    current=std::make_shared<QueryArtifacts>(artifacts,
        artifacts->directory+"/q"+std::to_string(active->id)+"-a"+std::to_string(++attempt));
    FILE *cnf=std::fopen((current->prefix+".cnf").c_str(),"w");
    if(!cnf)return {};
    const bool written=history.write_query_dimacs(active->id,cnf);
    const bool closed=std::fclose(cnf)==0;
    if(!written||!closed)return {};
    auto proof=std::make_unique<Proof>();
    if(!proof->open_automatic((current->prefix+".proof").c_str(),false))return {};
    return proof;
  }

  bool publish() {
    auto result=query->own_result_snapshot();
    if(!result || result->result==QueryResult::Unknown)return false;
    const bool sat=result->result==QueryResult::Sat;
    const auto certificate=sat?QueryCertificateStatus::Unchanged:
        write_query_up_certificate(history,result->query,current->prefix,&stopped);
    if(certificate==QueryCertificateStatus::Error || certificate==QueryCertificateStatus::Interrupted)return false;
    const auto identity=input_digest.finish();
    const std::string payload=current->prefix+(sat?".model":".core");
    FILE *file=std::fopen(payload.c_str(),"w");
    if(!file)return false;
    bool good=true;
    if(sat) {
      good=std::fputs("v ",file)>=0;
      for(size_t v=0;v<result->model.size();++v) {
        const auto lit=result->model[v]>0?int32_t(v+1):-int32_t(v+1);
        good=(std::fprintf(file,"%d ",lit)>0)&&good;
      }
    } else for(int32_t lit:result->core.assumptions)
      good=(std::fprintf(file,"%d ",lit)>0)&&good;
    good=(std::fputs("0\n",file)>=0)&&good;
    good=(std::fclose(file)==0)&&good;
    if(!good)return false;
    const bool reduced=!sat && result->core.kind==CoreKind::LicensedSubset;
    if(reduced && !write_core_certificate(history,*result,current->prefix+"-core"))return false;
    using strategy::controller::feedback_file_sha256;
    std::vector<std::string> members={".cnf",".proof",sat?".model":".core"},hashes;
    if(reduced) {members.push_back("-core.cnf");members.push_back("-core.proof");}
    for(const auto &member:members) {
      std::string digest;
      if(!feedback_file_sha256((current->prefix+member).c_str(),digest))return false;
      hashes.push_back(std::move(digest));
    }
    file=std::fopen((current->prefix+".json").c_str(),"w");
    if(!file)return false;
    // Relative member suffixes avoid embedding caller paths in JSON strings.
    good=std::fprintf(file,
      "{\"schema\":\"tactix-ipasir-query-v1\",\"query\":%llu,\"generation\":%llu,"
      "\"result\":%u,\"core\":\"%s\",\"proof_source\":\"%s\",\"permanent_encoding\":\"tactix-permanent-input-i32le-v1\","
      "\"permanent_sha256\":\"%s\",\"sha256\":{",
      static_cast<unsigned long long>(result->query),
      static_cast<unsigned long long>(result->generation),unsigned(result->result),
      reduced?"licensed-subset":"full-original-cube",
      certificate==QueryCertificateStatus::Written?"input-up":"native",identity.c_str())>0;
    for(size_t i=0;i<members.size();++i)
      good=(std::fprintf(file,"%s\"%s\":\"%s\"",i?",":"",members[i].c_str(),hashes[i].c_str())>0)&&good;
    good=(std::fputs("}}\n",file)>=0)&&good;
    good=(std::fclose(file)==0)&&good;
    if(good) {
      auto view=std::make_shared<PublishedResultView>(PublishedResultView{
          result,history.query(result->query)->assumptions,current,identity});
      retained.push_back(current);published=std::move(view);
    }
    return good;
  }

  void clear_execution() {
    if(execution)api_detail::destroy_query_execution(execution);
    execution=nullptr;
  }
  bool make_execution() {
    auto &settings=native&&!native->demo_kind?native->options:options;
    if(!execution)execution=api_detail::create_query_execution(*query,policy,settings,&stopped);
    return execution!=nullptr;
  }
  int unknown() {
    const bool unsettled=execution && !api_detail::query_execution_settled(execution);
    clear_execution();
    if(unsettled)query->discard_epoch();
    if(const auto *active=history.active_query())history.finish_query(active->id,QueryResult::Unknown);
    busy=false;return 0;
  }
  int advance_query(bool repair) {
    // The old writer may survive a rejected advance/replay. Keep its files
    // until replacement succeeds, and close failed writers before cleanup.
    struct Attempt {
      Session &session;
      std::shared_ptr<QueryArtifacts> previous;
      bool committed=false;
      ~Attempt() {if(!committed)session.current=std::move(previous);}
    } transaction{*this,current};
    auto proof=writer();if(!proof)return unknown();
    const auto advanced=repair?query->repair(history,std::move(proof),&stopped):
        query->advance(history,std::move(proof),&stopped);
    if(advanced.status==NativeAdvanceStatus::Interrupted)return unknown();
    if(advanced.status!=NativeAdvanceStatus::Ok && advanced.status!=NativeAdvanceStatus::Replayed) {
      proof=writer();if(!proof)return unknown();
      if(query->replay(history,std::move(proof))!=ReplayStatus::Ok)return unknown();
    }
    transaction.committed=true;return 1;
  }
  int prepare(bool expose) {
    if(busy || input_error)return -1;
    if(history.begin_query()!=InputStatus::Ok)return -1;
    clear_published();busy=true;stopped=0;query->reset_termination();
    if(query->poll_terminate(&stopped))return unknown();
    if(native&&!native->begin(options,history,*query,&stopped))return unknown();
    if(advance_query(false)!=1)return 0;
    if(expose && !make_execution())return unknown();
    return 1;
  }
  int repair_execution(bool expose=true) {
    if(!busy || !history.active_query() ||
       (execution && !api_detail::query_execution_settled(execution)))return -1;
    if(query->projection_state(query->ticket()).restore==false)return -1;
    clear_execution();
    if(advance_query(true)!=1)return 0;
    return !expose || make_execution()?1:unknown();
  }
  int finish(int result) {
    if(!busy || !history.active_query() || (result!=0 && result!=10 && result!=20))return -1;
    if(execution && !api_detail::query_execution_settled(execution))return -1;
    if(query->poll_terminate(&stopped))result=0;
    if(execution && api_detail::query_execution_status(execution)!=QuerySolveStatus::Complete && result)
      return -1;
    const auto answer=static_cast<QueryResult>(result);
    // Capture validates the exact original model/refutation; a strategy's
    // return value alone cannot establish a terminal result.
    const auto captured=query->finish_query(history,answer,{native_limits().proof,core_ticks},&stopped);
    if(captured==ResultCaptureStatus::Interrupted)return unknown();
    if(captured!=ResultCaptureStatus::Ok)return -1;
    clear_execution();busy=false;
    if(!result)return 0;
    if(!publish()) {input_error=true;return 0;}
    return result;
  }
  int program_boundary(bool install_assumptions) {
    for(;;) {
      tactix_ipasir_query_state state{};
      state.version=TACTIX_IPASIR_SESSION_VERSION;state.size=sizeof state;
      if(tactix_ipasir_next(this,&state))return 0;
      switch(state.action) {
      case TACTIX_QUERY_RESTORE:case TACTIX_QUERY_INTERRUPTED:
      case TACTIX_QUERY_INVALID:case TACTIX_QUERY_BUSY:return 0;
      case TACTIX_QUERY_ASSUME:
        if(!install_assumptions)return -1;
        if(tactix_ipasir_assume_next(this))return 0;
        break;
      case TACTIX_QUERY_BACKTRACK: {
        if(!install_assumptions)return -1;
        int restart=-1;
        for(int i=0;i<tactix_rule_count();++i)
          if(!std::strcmp(tactix_rule_name(i),"Restart")) {restart=i;break;}
        if(tactix_apply_instance(static_cast<::tactix_solver *>(execution),restart,0)<0)return 0;
        break;
      }
      case TACTIX_QUERY_SAT:case TACTIX_QUERY_UNSAT:
        return install_assumptions?state.action:-1;
      default:return -1;
      }
    }
  }
  int solve() {
    const bool programmed=strategy||native;
    if(prepare(programmed)!=1)return 0;
    unsigned cause=TACTIX_IPASIR_ENTER_QUERY;
    for(;;) {
      QuerySolveResult answer;
      if(programmed) {
        in_strategy=true;
        int proposed;
        if(native) {
          native->query_entries+=cause==TACTIX_IPASIR_ENTER_QUERY;
          native->restored_entries+=cause==TACTIX_IPASIR_ENTER_RESTORED;
          auto *view=static_cast<::tactix_solver *>(execution);
          if(native->demo_kind==NativeRuntime::chvatal) {
            (void)tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE);
            proposed=chvatal_run(&native->twins,view,[](void *session) {
              return int(static_cast<Session *>(session)->program_boundary(false)==0);
            },this,nullptr);
            if(proposed==CHVATAL_HANDOFF) {
              ++native->handoffs;proposed=tactix_solve(view);
            }
          } else if(native->demo_kind==NativeRuntime::scripted_default) {
            (void)tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE);
            proposed=tactix_scripted_default_run(&native->scripted,view,[](void *session) {
              return static_cast<Session *>(session)->program_boundary(true);
            },this);
          } else if(native->demo_kind) {
            proposed=tactix_search_demo_run(&native->search,view,[](void *session) {
              return static_cast<Session *>(session)->program_boundary(true);
            },this);
          } else {
            (void)tactix_set_guidance_publication(view,TACTIX_GUIDANCE_NATIVE);
            proposed=cruxmiter_run(&native->program,view,[](void *session) {
              return int(static_cast<Session *>(session)->program_boundary(false)==0);
            },this,nullptr);
            if(proposed==CRUXMITER_HANDOFF) {
              ++native->handoffs;proposed=tactix_solve(view);
            }
          }
        } else proposed=strategy(strategy_state,this,static_cast<::tactix_solver *>(execution),cause);
        in_strategy=false;
        if(proposed<0)return unknown();
        answer={api_detail::query_execution_status(execution),proposed};
        if(query->projection_state(query->ticket()).restore)answer.status=QuerySolveStatus::RestorationRequired;
      } else {
        ScopedStrategySettings settings(&options);
        answer=policy.solve_query(*query,&stopped);
      }
      if(answer.status==QuerySolveStatus::RestorationRequired) {
        // Discard the old borrowed view before replacing its proof namespace.
        if(repair_execution(programmed)!=1)return unknown();
        cause=TACTIX_IPASIR_ENTER_RESTORED;continue;
      }
      const int result=answer.status==QuerySolveStatus::Complete?answer.result:0;
      const int finished=finish(result);
      return finished<0?unknown():finished;
    }
  }
};
Session *as_session(void *p) {return static_cast<Session *>(p);}
int query_action(QuerySolveStatus status,int ready=TACTIX_QUERY_READY) {
  switch(status) {
  case QuerySolveStatus::InvalidState:case QuerySolveStatus::InvalidModel:case QuerySolveStatus::ProofFailure:
    return TACTIX_QUERY_INVALID;
  case QuerySolveStatus::Interrupted:return TACTIX_QUERY_INTERRUPTED;
  case QuerySolveStatus::RestorationRequired:return TACTIX_QUERY_RESTORE;
  default:return ready;
  }
}
}

struct tactix_ipasir_result {
  std::shared_ptr<const PublishedResultView> view;
};

struct tactix_ipasir_reduction {
  struct Probe {
    int status=0;
    std::vector<int32_t> assumptions;
    std::unique_ptr<tactix_ipasir_result> result;
  };
  std::unique_ptr<tactix_ipasir_result> source;
  std::vector<int32_t> context,core;
  std::vector<Probe> probes;
  std::vector<std::pair<int32_t,size_t>> witnesses;
  size_t certificate=SIZE_MAX; // source until a successful UNSAT deletion
  bool minimal=false;
};

namespace {
int snapshot_value(const QueryResultSnapshot *r,int literal) {
  if(!r || r->result!=QueryResult::Sat || !literal || literal==INT32_MIN)return 0;
  const auto variable=uint32_t(literal<0?-literal:literal);
  if(variable>r->model.size() || !r->model[variable-1])return 0;
  return r->model[variable-1]>0?int(variable):-int(variable);
}
bool snapshot_failed(const QueryResultSnapshot *r,int literal) {
  return r && r->failed_literal(literal);
}
tactix_ipasir_result *clone_result(const tactix_ipasir_result *r) {
  return r?new tactix_ipasir_result(*r):nullptr;
}
}

extern "C" {
const char *ipasir_signature(void) {return "tactix 0.2.1 experimental query transport";}
void *ipasir_init(void) {
  auto owner=std::make_unique<Session>();
  return owner->initialize()?owner.release():nullptr;
}
void ipasir_release(void *p) {if(p&&!as_session(p)->busy&&!as_session(p)->minimizing)delete as_session(p);}
void ipasir_add(void *p,int literal) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return;
  auto &s=*as_session(p);
  if(s.history.add(literal)!=InputStatus::Ok) {s.input_error=true;return;}
  s.hash_input_suffix();s.clear_published();
}
void ipasir_assume(void *p,int literal) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return;
  auto &s=*as_session(p);
  if(s.history.assume(literal)!=InputStatus::Ok) {s.input_error=true;return;}
  s.clear_published();
}
int ipasir_solve(void *p) {return p&&!as_session(p)->minimizing?as_session(p)->solve():0;}
int ipasir_val(void *p,int literal) {
  return p&&as_session(p)->published?snapshot_value(as_session(p)->published->snapshot.get(),literal):0;
}
int ipasir_failed(void *p,int literal) {
  return p&&as_session(p)->published?snapshot_failed(as_session(p)->published->snapshot.get(),literal):0;
}
void ipasir_set_terminate(void *p,void *state,int (*terminate)(void *)) {
  if(p&&!as_session(p)->busy&&!as_session(p)->minimizing)as_session(p)->query->set_terminate(state,terminate);
}
void ipasir_set_learn(void *p,void *state,int maximum_length,void (*learn)(void *,int *)) {
  if(p&&!as_session(p)->busy&&!as_session(p)->minimizing)as_session(p)->query->set_learn(state,maximum_length,learn);
}
int tactix_ipasir_set_learn_budget(void *p,uint64_t ticks) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return -1;
  as_session(p)->query->set_learn_budget(ticks);return 0;
}
const char *tactix_ipasir_bundle(void *p) {
  return p&&as_session(p)->published?as_session(p)->published->files->prefix.c_str():nullptr;
}
tactix_ipasir_result *tactix_ipasir_result_acquire(void *p) {
  if(!p)return nullptr;
  const auto &s=*as_session(p);
  if(s.busy || !s.published)return nullptr;
  return new tactix_ipasir_result{s.published};
}
void tactix_ipasir_result_release(tactix_ipasir_result *r) {delete r;}
int tactix_ipasir_result_status(const tactix_ipasir_result *r) {
  return r?int(r->view->snapshot->result):0;
}
size_t tactix_ipasir_result_variables(const tactix_ipasir_result *r) {
  return r&&r->view->snapshot->result==QueryResult::Sat?r->view->snapshot->model.size():0;
}
int tactix_ipasir_result_val(const tactix_ipasir_result *r,int literal) {
  return snapshot_value(r?r->view->snapshot.get():nullptr,literal);
}
int tactix_ipasir_result_failed(const tactix_ipasir_result *r,int literal) {
  return snapshot_failed(r?r->view->snapshot.get():nullptr,literal);
}
size_t tactix_ipasir_result_assumptions(const tactix_ipasir_result *r) {
  return r?r->view->assumptions.size():0;
}
int tactix_ipasir_result_assumption(const tactix_ipasir_result *r,size_t index) {
  return r&&index<r->view->assumptions.size()?r->view->assumptions[index]:0;
}
size_t tactix_ipasir_result_core_size(const tactix_ipasir_result *r) {
  return r&&r->view->snapshot->result==QueryResult::Unsat?r->view->snapshot->core.assumptions.size():0;
}
int tactix_ipasir_result_core_literal(const tactix_ipasir_result *r,size_t index) {
  return r&&r->view->snapshot->result==QueryResult::Unsat&&index<r->view->snapshot->core.assumptions.size()
      ?r->view->snapshot->core.assumptions[index]:0;
}
const char *tactix_ipasir_result_bundle(const tactix_ipasir_result *r) {
  return r?r->view->files->prefix.c_str():nullptr;
}
const char *tactix_ipasir_result_input_sha256(const tactix_ipasir_result *r) {
  return r?r->view->input_hash.c_str():nullptr;
}
tactix_ipasir_reduction *tactix_ipasir_minimize_core(void *p,const tactix_ipasir_result *source,
    const int *context,size_t context_size,uint64_t max_probes) {
  if(!p || !source || (context_size&&!context))return nullptr;
  auto &s=*as_session(p);
  if(s.busy || s.minimizing || s.input_error || s.history.active_query() ||
      !s.history.pending_clause().empty() || !s.history.pending_assumptions().empty() ||
      source->view->snapshot->result!=QueryResult::Unsat || source->view->files->store!=s.artifacts ||
      source->view->snapshot->input_words!=s.history.input().size() ||
      source->view->input_hash!=s.input_digest.finish())return nullptr;
  auto reduced=std::make_unique<tactix_ipasir_reduction>();
  reduced->source.reset(clone_result(source));
  const auto &a=reduced->source->view->assumptions;
  std::unordered_set<int32_t> original(a.begin(),a.end()),fixed,seen;
  for(size_t i=0;i<context_size;++i) {
    const int literal=context[i];
    if(!literal || literal==INT32_MIN || !original.count(literal))return nullptr;
    if(fixed.insert(literal).second)reduced->context.push_back(literal);
  }
  for(int literal:a)
    if(!fixed.count(literal)&&seen.insert(literal).second)reduced->core.push_back(literal);
  // Preserve the actual immutable view object, including borrowed c_str/data
  // addresses. The kernel/history may advance while that published theorem
  // remains valid for this unchanged permanent prefix.
  struct RestoreView {
    Session &session;
    std::shared_ptr<const PublishedResultView> saved;
    explicit RestoreView(Session &s):session(s),saved(s.published) {s.minimizing=true;}
    ~RestoreView() {
      (void)session.history.discard_pending_assumptions();
      session.published=std::move(saved);session.minimizing=false;
    }
  } restore(s);
  size_t position=0;
  while(position<reduced->core.size()) {
    if(reduced->probes.size()>=max_probes)return reduced.release();
    tactix_ipasir_reduction::Probe probe;
    probe.assumptions=reduced->context;
    for(size_t i=0;i<reduced->core.size();++i)
      if(i!=position)probe.assumptions.push_back(reduced->core[i]);
    bool queued=true;
    for(int literal:probe.assumptions)
      if(s.history.assume(literal)!=InputStatus::Ok) {queued=false;break;}
    probe.status=queued?s.solve():0;
    if(probe.status)probe.result.reset(tactix_ipasir_result_acquire(p));
    if(probe.status&&!probe.result)probe.status=0;
    const int status=probe.status;
    const size_t index=reduced->probes.size();
    reduced->probes.push_back(std::move(probe));
    if(!status)return reduced.release();
    if(status==20) {
      reduced->core.erase(reduced->core.begin()+position);reduced->certificate=index;
    } else {
      reduced->witnesses.emplace_back(reduced->core[position],index);++position;
    }
  }
  reduced->minimal=true;return reduced.release();
}
void tactix_ipasir_reduction_release(tactix_ipasir_reduction *r) {delete r;}
int tactix_ipasir_reduction_minimal(const tactix_ipasir_reduction *r) {return r&&r->minimal;}
size_t tactix_ipasir_reduction_core_size(const tactix_ipasir_reduction *r) {return r?r->core.size():0;}
int tactix_ipasir_reduction_core_literal(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->core.size()?r->core[index]:0;
}
size_t tactix_ipasir_reduction_context_size(const tactix_ipasir_reduction *r) {return r?r->context.size():0;}
int tactix_ipasir_reduction_context_literal(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->context.size()?r->context[index]:0;
}
tactix_ipasir_result *tactix_ipasir_reduction_source(const tactix_ipasir_reduction *r) {
  return r?clone_result(r->source.get()):nullptr;
}
tactix_ipasir_result *tactix_ipasir_reduction_certificate(const tactix_ipasir_reduction *r) {
  return r?clone_result(r->certificate==SIZE_MAX?r->source.get():r->probes[r->certificate].result.get()):nullptr;
}
size_t tactix_ipasir_reduction_probes(const tactix_ipasir_reduction *r) {return r?r->probes.size():0;}
int tactix_ipasir_reduction_probe_status(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->probes.size()?r->probes[index].status:0;
}
size_t tactix_ipasir_reduction_probe_assumptions(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->probes.size()?r->probes[index].assumptions.size():0;
}
int tactix_ipasir_reduction_probe_assumption(const tactix_ipasir_reduction *r,size_t index,size_t assumption) {
  return r&&index<r->probes.size()&&assumption<r->probes[index].assumptions.size()
      ?r->probes[index].assumptions[assumption]:0;
}
tactix_ipasir_result *tactix_ipasir_reduction_probe_result(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->probes.size()?clone_result(r->probes[index].result.get()):nullptr;
}
size_t tactix_ipasir_reduction_witnesses(const tactix_ipasir_reduction *r) {return r?r->witnesses.size():0;}
int tactix_ipasir_reduction_witness_literal(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->witnesses.size()?r->witnesses[index].first:0;
}
size_t tactix_ipasir_reduction_witness_probe(const tactix_ipasir_reduction *r,size_t index) {
  return r&&index<r->witnesses.size()?r->witnesses[index].second:0;
}
int tactix_ipasir_set_core_budget(void *p,uint64_t ticks) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return -1;
  as_session(p)->core_ticks=ticks;return 0;
}
int tactix_ipasir_set_strategy(void *p,void *state,tactix_ipasir_strategy strategy) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return -1;
  as_session(p)->native.reset();
  as_session(p)->strategy_state=state;as_session(p)->strategy=strategy;return 0;
}
int tactix_ipasir_set_strategy_name(void *p,const char *name) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return -1;
  auto &s=*as_session(p);
  if(!name || !std::strcmp(name,"default")) {
    s.native.reset();s.strategy=nullptr;s.strategy_state=nullptr;return 0;
  }
  const int kind=!std::strcmp(name,"dpll")?TACTIX_DEMO_DPLL:
      !std::strcmp(name,"cdcl-no-restarts")?TACTIX_DEMO_NO_RESTARTS:
      !std::strcmp(name,"chvatal")?NativeRuntime::chvatal:
      !std::strcmp(name,"scripted-default")?NativeRuntime::scripted_default:0;
  if(!kind && std::strcmp(name,"cruxmiter"))return -1;
  auto runtime=std::make_unique<NativeRuntime>();runtime->demo_kind=kind;
  const bool valid=kind==NativeRuntime::chvatal?true:kind==NativeRuntime::scripted_default?
      (tactix_scripted_default_begin(&runtime->scripted),true):
      kind?tactix_search_demo_begin(&runtime->search,kind):cruxmiter_begin(&runtime->program);
  if(!valid)return -1;
  s.native=std::move(runtime);s.strategy=nullptr;s.strategy_state=nullptr;return 0;
}
const char *tactix_ipasir_strategy_name(void *p) {
  if(!p || as_session(p)->busy || as_session(p)->minimizing)return nullptr;
  const auto &s=*as_session(p);
  return s.native?(s.native->demo_kind==TACTIX_DEMO_DPLL?"dpll":
      s.native->demo_kind==NativeRuntime::chvatal?"chvatal":
      s.native->demo_kind==NativeRuntime::scripted_default?"scripted-default":
      s.native->demo_kind==TACTIX_DEMO_NO_RESTARTS?"cdcl-no-restarts":"cruxmiter"):
      s.strategy?"callback":"default";
}
int tactix_ipasir_get_strategy_counters(void *p,tactix_ipasir_strategy_counters *out) {
  if(!p || !out || out->version!=TACTIX_IPASIR_SESSION_VERSION || out->size<sizeof *out ||
      as_session(p)->busy || as_session(p)->minimizing || !as_session(p)->native)return -1;
  const auto &s=*as_session(p)->native;
  out->query_entries=s.query_entries;out->restored_entries=s.restored_entries;
  out->root_requests=s.requests+s.program.requests+s.twins.requests;
  out->root_eligible=s.eligible+s.program.eligible+s.twins.eligible;
  out->root_effective=s.effective+s.program.effective+s.twins.effective;
  out->root_rounds=s.rounds+s.program.rounds+s.twins.completed_rounds;out->default_handoffs=s.handoffs;
  return 0;
}
int tactix_ipasir_set_default_option(void *p,const char *name,const char *value) {
  return p&&!as_session(p)->busy&&!as_session(p)->minimizing?api_detail::set_strategy_option(as_session(p)->options,name,value):-1;
}
int tactix_ipasir_begin(void *p,::tactix_solver **execution) {
  if(!execution)return -1;
  *execution=nullptr;if(!p||as_session(p)->minimizing)return -1;
  const int prepared=as_session(p)->prepare(true);
  if(prepared==1)*execution=static_cast<::tactix_solver *>(as_session(p)->execution);
  return prepared;
}
uint64_t tactix_ipasir_rejected_models(void *p) {
  if(!p)return UINT64_MAX;
  const auto &s=*as_session(p);
  if(!s.busy || !s.execution)return UINT64_MAX;
  return s.query->projection_state(s.query->ticket()).rejected_models;
}
int tactix_ipasir_next(void *p,tactix_ipasir_query_state *state) {
  if(!p || !state || state->version!=TACTIX_IPASIR_SESSION_VERSION || state->size<sizeof *state)return -1;
  auto &s=*as_session(p);
  if(!s.busy || !s.execution)return -1;
  state->assumption_index=0;state->literal=0;
  state->generation=s.query->ticket().generation;
  if(s.query->poll_terminate(&s.stopped)) {state->action=TACTIX_QUERY_INTERRUPTED;return 0;}
  state->action=query_action(api_detail::query_execution_status(s.execution));
  if(state->action!=TACTIX_QUERY_READY)return 0;
  const auto next=s.query->next_assumption(s.query->ticket());
  state->assumption_index=next.index;state->literal=next.literal;
  switch(next.status) {
  case AssumptionStatus::Ready: {
    const int terminal=tactix_status(static_cast<::tactix_solver *>(s.execution));
    // Status reconstruction polls termination too. Honor the state it latched
    // during this same observation instead of publishing a stale READY action.
    state->action=query_action(api_detail::query_execution_status(s.execution),
                              terminal?terminal:TACTIX_QUERY_READY);
    break;
  }
  case AssumptionStatus::Decide:state->action=TACTIX_QUERY_ASSUME;break;
  case AssumptionStatus::Propagate:state->action=TACTIX_QUERY_PROPAGATE;break;
  case AssumptionStatus::Analyze:state->action=TACTIX_QUERY_ANALYZE;break;
  case AssumptionStatus::Backtrack:state->action=TACTIX_QUERY_BACKTRACK;break;
  case AssumptionStatus::Restore:state->action=TACTIX_QUERY_RESTORE;break;
  case AssumptionStatus::BaseUnsat:case AssumptionStatus::QueryUnsat:state->action=TACTIX_QUERY_UNSAT;break;
  case AssumptionStatus::Busy:state->action=TACTIX_QUERY_BUSY;break;
  default:state->action=TACTIX_QUERY_INVALID;break;
  }
  return 0;
}
int tactix_ipasir_assume_next(void *p) {
  if(!p || !as_session(p)->busy || !as_session(p)->execution)return -1;
  auto &s=*as_session(p);
  if(s.query->poll_terminate(&s.stopped))return TACTIX_IPASIR_ACTION_INTERRUPTED;
  const auto next=s.query->next_assumption(s.query->ticket());
  return next.status==AssumptionStatus::Decide && s.query->decide_assumption(s.query->ticket(),next.index)?0:-1;
}
int tactix_ipasir_repair(void *p,::tactix_solver **execution) {
  if(!p || !execution || as_session(p)->in_strategy || as_session(p)->minimizing)return -1;
  *execution=nullptr;const int repaired=as_session(p)->repair_execution();
  if(repaired==1)*execution=static_cast<::tactix_solver *>(as_session(p)->execution);
  return repaired;
}
int tactix_ipasir_finish(void *p,int result) {
  return p&&!as_session(p)->in_strategy&&!as_session(p)->minimizing?as_session(p)->finish(result):-1;
}
}
