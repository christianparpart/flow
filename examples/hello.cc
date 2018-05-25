// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/lang/Interpreter.h>
#include <flow/Diagnostics.h>
#include <iostream>

int main(int argc, const char* argv[]) {
  flow::lang::Interpreter interpreter;
  flow::diagnostics::ConsoleReport report;

  interpreter.registerFunction("greetings")
      .param<std::string>("from")
      .bind([](flow::Params& params) {
    std::cout << "Hello, " << params.getString(1) << "!\n";
  });

  if (interpreter.compileString(R"(handler greeter {
                                     greetings from: "World";
                                   };
                                  )", &report)) {
    interpreter.run("greeter");
  }
}
