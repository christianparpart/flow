// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/lang/AST.h>
#include <flow/lang/IRGenerator.h>
#include <flow/lang/Interpreter.h>
#include <flow/lang/Parser.h>

#include <flow/TargetCodeGenerator.h>
#include <flow/ir/IRProgram.h>
#include <flow/ir/PassManager.h>
#include <flow/transform/EmptyBlockElimination.h>
#include <flow/transform/InstructionElimination.h>
#include <flow/transform/MergeBlockPass.h>
#include <flow/transform/UnusedBlockPass.h>
#include <flow/vm/Runner.h>
#include <flow/vm/Runtime.h>

namespace flow::lang {

Interpreter::Interpreter()
    : programIR_{},
      initialized_{false},
      program_{} {
}

bool Interpreter::compileString(const std::string& source,
                                diagnostics::Report* report,
                                int optimizationLevel) {
  Parser parser({Feature::GlobalScope, Feature::WhileLoop},
                report,
                this,
                std::bind(&Runtime::import, this, std::placeholders::_1,
                                                  std::placeholders::_2,
                                                  std::placeholders::_3));
  parser.openString(source);
  return compile(std::move(parser), report, optimizationLevel);
}

bool Interpreter::compileLocalFile(const std::string& path,
                                   diagnostics::Report* report,
                                   int optimizationLevel) {
  Parser parser({Feature::GlobalScope, Feature::WhileLoop},
                report,
                this,
                std::bind(&Runtime::import, this, std::placeholders::_1,
                                                  std::placeholders::_2,
                                                  std::placeholders::_3));
  parser.openLocalFile(path);
  return compile(std::move(parser), report, optimizationLevel);
}

bool Interpreter::compile(Parser&& parser,
                          diagnostics::Report* report,
                          int optimizationLevel) {
  std::unique_ptr<flow::lang::UnitSym> unit = parser.parse();

  if (report->errorCount() > 0)
    return false;

  IRGenerator irgen{report};
  std::unique_ptr<IRProgram> programIR = irgen.generate(unit.get());

  {
    flow::PassManager pm;

    // mandatory passes
    pm.registerPass(std::make_unique<flow::UnusedBlockPass>());

    // optional passes
    if (optimizationLevel >= 1) {
      pm.registerPass(std::make_unique<flow::MergeBlockPass>());
      pm.registerPass(std::make_unique<flow::EmptyBlockElimination>());
      pm.registerPass(std::make_unique<flow::InstructionElimination>());
    }

    pm.run(programIR.get());
  }

  programIR_ = std::move(programIR);

  std::unique_ptr<Program> program = TargetCodeGenerator().generate(programIR_.get());
  program->link(this, report);
  if (report->errorCount() > 0)
    return false;

  program_ = std::move(program);
  initialized_ = false;
  return true;
}

#define GLOBAL_SCOPE_INIT_NAME "@__global_init__"

bool Interpreter::run(const std::string& handlerName, void* userdata, TraceLogger trace) const {
  if (!initialized_) {
    initialized_ = true;
    if (Handler* handler = program_->findHandler(GLOBAL_SCOPE_INIT_NAME); handler != nullptr) {
      Runner{handler, userdata, &globals_, trace}.run();
    }
  }

  Handler* handler = program_->findHandler(handlerName);
  if (!handler)
    return false;

  return Runner{handler, userdata, &globals_, trace}.run();
}

} // namespace flow::lang
