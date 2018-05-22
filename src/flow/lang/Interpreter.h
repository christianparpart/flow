// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <flow/NativeCallback.h>
#include <flow/Params.h>
#include <flow/ir/IRProgram.h>
#include <flow/vm/Program.h>
#include <flow/vm/Runner.h>
#include <flow/vm/Runtime.h>

#include <memory>

namespace flow::diagnostics {
  class Report;
}

namespace flow::lang {

class Parser;

/**
 * Convenience API for compiling and running Flow scripts.
 */
class Interpreter : public Runtime {
 public:
  using TraceLogger = Runner::TraceLogger;

  Interpreter();

  bool compileString(const std::string& path, diagnostics::Report* report, int optimizationLevel = 0);
  bool compileLocalFile(const std::string& path, diagnostics::Report* report, int optimizationLevel = 0);

  bool run(const std::string& handlerName, void* userdata = nullptr, TraceLogger trace = TraceLogger{}) const;

  Program* program() const noexcept { return program_.get(); }
  IRProgram* programIR() const noexcept { return programIR_.get(); }

 private:
  bool compile(Parser&& parser, diagnostics::Report* report, int optimizationLevel);

 private:
  std::unique_ptr<IRProgram> programIR_;
  mutable bool initialized_;
  mutable Runner::Globals globals_;
  std::unique_ptr<Program> program_;
};

} // namespace flow
