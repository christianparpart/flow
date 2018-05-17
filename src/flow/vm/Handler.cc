// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/vm/Handler.h>
#include <flow/vm/Program.h>
#include <flow/vm/Runner.h>
#include <flow/vm/Instruction.h>
#include <flow/sysconfig.h>

namespace flow {

Handler::Handler(Program* program,
                 std::string name,
                 std::vector<Instruction> code)
    : program_(program),
      name_(std::move(name)),
      stackSize_(),
      code_()
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
      ,
      directThreadedCode_()
#endif
{
  setCode(std::move(code));
}

void Handler::setCode(std::vector<Instruction> code) {
  code_ = std::move(code);

  if (opcode(code_.back()) != Opcode::EXIT)
    code_.push_back(makeInstruction(Opcode::EXIT, false));

  stackSize_ = computeStackSize(code_.data(), code_.size());

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  directThreadedCode_.clear();
#endif
}

void Handler::disassemble() const noexcept {
  printf("\n.handler %-27s ; (%zu stack size, %zu instructions)\n",
         name().c_str(),
         stackSize(),
         code().size());
  printf("%s", flow::disassemble(code_.data(), code_.size(), "  ",
                                 &program_->constants()).c_str());
}

}  // namespace flow
