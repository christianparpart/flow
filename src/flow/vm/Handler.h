// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/vm/Instruction.h>
#include <flow/sysconfig.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace flow {

class Program;
class Runner;

class Handler {
 public:
  Handler(Program* program,
          std::string name,
          std::vector<Instruction> instructions);
  Handler() = default;
  Handler(const Handler& handler) = default;
  Handler(Handler&& handler) noexcept = default;
  Handler& operator=(const Handler& handler) = default;
  Handler& operator=(Handler&& handler) noexcept = default;
  ~Handler() = default;

  Program* program() const noexcept { return program_; }

  const std::string& name() const noexcept { return name_; }
  void setName(const std::string& name) { name_ = name; }

  size_t stackSize() const noexcept { return stackSize_; }

  const std::vector<Instruction>& code() const noexcept { return code_; }
  void setCode(std::vector<Instruction> code);

#if defined(FLOW_DIRECT_THREADED_VM)
  const std::vector<uint64_t>& directThreadedCode() const noexcept { return directThreadedCode_; }
  std::vector<uint64_t>& directThreadedCode() noexcept { return directThreadedCode_; }
#endif

  void disassemble() const noexcept;

 private:
  Program* program_;
  std::string name_;
  size_t stackSize_;
  std::vector<Instruction> code_;
#if defined(FLOW_DIRECT_THREADED_VM)
  std::vector<uint64_t> directThreadedCode_;
#endif
};

}  // namespace flow
