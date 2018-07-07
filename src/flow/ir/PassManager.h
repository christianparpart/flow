// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <functional>
#include <list>
#include <memory>
#include <utility>

#include <fmt/format.h>

namespace flow {

class IRHandler;
class IRProgram;

class PassManager {
 public:
  using HandlerPass = std::function<bool(IRHandler* handler)>;

  PassManager() = default;
  ~PassManager() = default;

  /** registers given pass to the pass manager.
   *
   * @param name uniquely identifyable name of the handler pass
   * @param handler callback to invoke to handle the transformation pass
   *
   * The handler must return @c true if it modified its input, @c false otherwise.
   */
  void registerPass(std::string name, HandlerPass handler);

  /** runs passes on a complete program.
   */
  void run(IRProgram* program);

  /** runs passes on given handler.
   */
  void run(IRHandler* handler);

  template<typename... Args>
  void logDebug(const char* msg, Args... args) {
    logDebug(fmt::format(msg, args...));
  }

  void logDebug(const std::string& msg);

 private:
  std::list<std::pair<std::string, HandlerPass>> handlerPasses_;
};

}  // namespace flow
