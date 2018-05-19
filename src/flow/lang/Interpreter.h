// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <flow/vm/Program.h>
#include <flow/vm/Runner.h>
#include <memory>

namespace flow::diagnostics {
  class Report;
}

namespace flow {
  class Runtime;
}

namespace flow::lang {

/**
 * Convenience API for compiling and running Flow scripts.
 */
class Interpreter {
 public:
  using TraceLogger = Runner::TraceLogger;

  //! Constructs an Interpreter for the given @p runtime.
  explicit Interpreter(Runtime* runtime);

  bool compile(const std::string& path, diagnostics::Report* report, int optimizationLevel = 0);

  bool run(const std::string& handlerName, void* userdata, TraceLogger trace = TraceLogger{}) const;

  Program* program() const noexcept { return program_.get(); }

 private:
  Runtime* runtime_;
  std::unique_ptr<Program> program_;
};

} // namespace flow
