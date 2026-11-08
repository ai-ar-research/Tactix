#!/usr/bin/env python3
"""Install the research-only probe into a new immutable build source pin."""
from pathlib import Path
import shutil,sys

def install(source):
 source=Path(source).resolve();root=Path.cwd().resolve()
 if root!=Path('/home/hwu/Tactix'):raise ValueError('run only from the authorized repository')
 if not source.is_relative_to(root/'build') or source==root:raise ValueError('requires a fresh source pin under this repository/build')
 for name in ['shared_anf.hpp','shared_nonlinear_probe.hpp']:shutil.copy2(Path(__file__).with_name(name),source/'src/diagnostics'/name)
 for name in ['test_shared_anf.cpp','test_shared_nonlinear_probe.cpp']:shutil.copy2(Path(__file__).with_name(name),source/'tests'/name)
 def edit(name,before,after):
  p=source/name;s=p.read_text()
  if s.count(before)!=1:raise ValueError('nonunique research insertion: '+before)
  p.write_text(s.replace(before,after))
 edit('src/policies/default_policy.cpp','#include "policies/default_policy.hpp"','#include "policies/default_policy.hpp"\n#include "diagnostics/shared_nonlinear_probe.hpp"')
 edit('src/policies/default_policy.cpp','  presearch_ticks->leave();','  if (!shared_nonlinear_probe::run(k)) return finish(20);\n  presearch_ticks->leave();')
 edit('src/policies/default_policy.cpp','    progress_.finish(k, verdict);','    shared_nonlinear_probe::emit(k,"finish");\n    progress_.finish(k, verdict);')
 edit('src/policies/default_policy.cpp','  return decision;\n}\n\nbool DefaultPolicy::affine_additional_recognition_due','  shared_nonlinear_probe::decision(k,decision);\n  return decision;\n}\n\nbool DefaultPolicy::affine_additional_recognition_due')
 edit('src/main.cpp','#include <csignal>','#include <csignal>\n#include "diagnostics/shared_nonlinear_probe.hpp"')
 edit('src/main.cpp','  Proof proof;','''  const char *nonlinear_arm = shared_nonlinear_probe::arm();
  if (std::strcmp(nonlinear_arm,"baseline") && std::strcmp(nonlinear_arm,"definitions") &&
      std::strcmp(nonlinear_arm,"priority") && std::strcmp(nonlinear_arm,"control") && std::strcmp(nonlinear_arm,"audit"))
    return fail_unknown("invalid nonlinear research arm\\n");
  if (shared_nonlinear_probe::active() && proof_path)
    return fail_unknown("nonlinear research currently forbids proof output\\n");
  shared_nonlinear_probe::no_proof_authorized = !proof_path;
  Proof proof;''')
 return source
if __name__=='__main__':install(sys.argv[1])
