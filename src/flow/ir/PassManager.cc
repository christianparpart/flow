// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/PassManager.h>
#include <flow/ir/IRProgram.h>
#include <fmt/format.h>
#include <cstdio>
#include <cstdlib>

namespace flow {

void PassManager::registerPass(std::string name, HandlerPass pass) {
  handlerPasses_.emplace_back(std::move(name), std::move(pass));
}

void PassManager::run(IRProgram* program) {
  for (IRHandler* handler : program->handlers()) {
    logDebug("optimizing handler {}", handler->name());
    run(handler);
  }
}

void PassManager::run(IRHandler* handler) {
  for (;;) {
    int changes = 0;
    for (const std::pair<std::string, HandlerPass>& pass : handlerPasses_) {
      logDebug("executing pass {}:", pass.first);
      if (pass.second(handler)) {
        logDebug("pass {}: changes detected", pass.first);
        handler->verify();
        changes++;
      }
    }
    logDebug("{} changes detected", changes);
    if (!changes) {
      break;
    }
  }
}

void PassManager::logDebug(const std::string& msg) {
  const char* debugFlag = getenv("FLOW_DEBUG_TRANSFORMS");
  if (debugFlag && strcmp(debugFlag, "1") == 0) {
    fprintf(stderr, "PassManager: %s\n", msg.c_str());
  }
}

}  // namespace flow
