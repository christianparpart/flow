// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "flowtest.h"

#include <flow/NativeCallback.h>
#include <flow/Params.h>
#include <flow/SourceLocation.h>
#include <flow/TargetCodeGenerator.h>
#include <flow/ir/IRProgram.h>
#include <flow/ir/PassManager.h>
#include <flow/lang/IRGenerator.h>
#include <flow/lang/Parser.h>
#include <flow/transform/EmptyBlockElimination.h>
#include <flow/transform/InstructionElimination.h>
#include <flow/transform/MergeBlockPass.h>
#include <flow/transform/UnusedBlockPass.h>
#include <flow/vm/Program.h>
#include <flow/vm/Runtime.h>

#include <fmt/format.h>

#include <experimental/filesystem>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::experimental::filesystem;

using namespace std;
using namespace flowtest;
using flow::SourceLocation;

class Tester : public flow::Runtime {
 public:
  Tester();

  bool test(const std::string& path);
  bool testFile(const std::string& filename);
  bool testDirectory(const std::string& path);

 private:
  void compileFile(const std::string& filename, flow::diagnostics::Report* report);

  bool import(const std::string& name,
              const std::string& path,
              std::vector<flow::NativeCallback*>* builtins) override;

  void reportError(const std::string& msg);

  template <typename... Args>
  void reportError(const std::string& msg, Args&&... args) {
    reportError(fmt::format(msg, args...));
  }

  // handlers
  void flow_handle_always(flow::Params& args);
  void flow_handle(flow::Params& args);

  // functions
  void flow_sum(flow::Params& args);
  void flow_assert(flow::Params& args);

  void flow_print(flow::Params& args);

 private:
  flow::diagnostics::BufferedReport report_;
  uintmax_t errorCount_ = 0;
  std::string output_;
};

Tester::Tester() {
  registerHandler("handle_always").bind(&Tester::flow_handle_always, this);

  registerHandler("handle").bind(&Tester::flow_handle, this).param<bool>("result");

  registerFunction("sum", flow::LiteralType::Number)
      .bind(&Tester::flow_sum, this)
      .param<flow::FlowNumber>("x")
      .param<flow::FlowNumber>("y");

  registerFunction("assert", flow::LiteralType::Number)
      .bind(&Tester::flow_assert, this)
      .param<flow::FlowNumber>("condition")
      .param<flow::FlowString>("description", "");

  registerFunction("print").bind(&Tester::flow_print, this).param<flow::FlowString>("text");
}

void Tester::flow_handle_always(flow::Params& args) {
  args.setResult(true);
}

void Tester::flow_handle(flow::Params& args) {
  args.setResult(args.getBool(1));
}

void Tester::flow_sum(flow::Params& args) {
  const flow::FlowNumber x = args.getInt(1);
  const flow::FlowNumber y = args.getInt(2);
  args.setResult(x + y);
}

void Tester::flow_assert(flow::Params& args) {
  const bool condition = args.getBool(1);
  const std::string& description = args.getString(2);

  if (!condition) {
    if (description.empty())
      reportError("Assertion failed.");
    else
      reportError(fmt::format("Assertion failed ({}).", description));
  }
}

void Tester::flow_print(flow::Params& args) {
  output_ += args.getString(1);
}

bool Tester::import(const std::string& name,
                    const std::string& path,
                    std::vector<flow::NativeCallback*>* builtins) {
  return true;
}

void Tester::reportError(const std::string& msg) {
  fmt::print("{}\n", msg);
  errorCount_++;
}

bool Tester::test(const std::string& path) {
  fs::path p{path};
  if (fs::is_directory(p))
    return testDirectory(path);

  if (fs::is_regular_file(p))
    return testFile(path);
  return false;
}

bool Tester::testDirectory(const std::string& path) {
  int errorCount = 0;
  for (auto& dir : fs::recursive_directory_iterator(path)) {
    if (dir.path().extension() == ".flow") {
      report_.clear();
      if (!testFile(dir.path().string())) {
        report_.log();
        errorCount++;
      }
    }
  }

  return errorCount == 0;
}

static std::string readFile(const std::string& filename) {
  std::ifstream f(filename);
  std::stringstream sstr;
  sstr << f.rdbuf();
  return std::move(sstr.str());
}

bool Tester::testFile(const std::string& filename) {
  flow::diagnostics::BufferedReport actual;
  compileFile(filename, &actual);

  flow::diagnostics::BufferedReport expected;
  Parser p(filename, readFile(filename));
  std::error_code ec = p.parse(&expected);
  if (ec) {
    reportError("Parse Error({}): {}", ec.category().name(), ec.message());
    return false;
  }

  flow::diagnostics::DifferenceReport diff = flow::diagnostics::difference(actual, expected);
  for (const Message& m : diff.first)
    reportError("Missing: {}", m);
  for (const Message& m : diff.second)
    reportError("Superfluous: {}", m);

  if (actual != expected)
    return false;

  return true;
}

void Tester::compileFile(const std::string& filename, flow::diagnostics::Report* report) {
  fmt::print("testing {}\n", filename);

  constexpr bool optimize = true;
  flow::lang::Parser parser{{flow::lang::Feature::GlobalScope, flow::lang::Feature::WhileLoop},
                            report,
                            this,
                            [this](auto x, auto y, auto z) { return import(x, y, z); }};
  parser.openStream(std::make_unique<std::ifstream>(filename), filename);
  std::unique_ptr<flow::lang::UnitSym> unit = parser.parse();

  flow::lang::IRGenerator irgen(report, {"main"});
  std::shared_ptr<flow::IRProgram> programIR = irgen.generate(unit.get());

  if (optimize) {
    flow::PassManager pm;

    pm.registerPass("eliminate-empty-blocks", &flow::transform::emptyBlockElimination);
    pm.registerPass("eliminate-linear-br", &flow::transform::eliminateLinearBr);
    pm.registerPass("eliminate-unused-blocks", &flow::transform::eliminateUnusedBlocks);
    pm.registerPass("eliminate-unused-instr", &flow::transform::eliminateUnusedInstr);
    pm.registerPass("fold-constant-condbr", &flow::transform::foldConstantCondBr);
    pm.registerPass("rewrite-br-to-exit", &flow::transform::rewriteBrToExit);
    pm.registerPass("rewrite-cond-br-to-same-branches", &flow::transform::rewriteCondBrToSameBranches);

    pm.run(programIR.get());
  }

  std::unique_ptr<flow::Program> program = flow::TargetCodeGenerator().generate(programIR.get());

  program->link(this, report);

  // TODO: execute to check expected output against `output_`
  output_.clear();
}

int main(int argc, const char* argv[]) {
  Tester t;
  bool success = t.test(argv[1]);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
