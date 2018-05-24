// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/IRProgram.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/ConstantArray.h>

#include <cassert>

namespace flow {

IRProgram::IRProgram()
    : modules_(),
      trueLiteral_(true, "trueLiteral"),
      falseLiteral_(false, "falseLiteral"),
      numbers_(),
      strings_(),
      ipaddrs_(),
      cidrs_(),
      regexps_(),
      builtinFunctions_(),
      builtinHandlers_(),
      handlers_() {
}

#define GLOBAL_SCOPE_INIT_NAME "@__global_init__"

IRProgram::~IRProgram() {
  // first reset all standard handlers and *then* the global-scope initialization handler
  // in order to not cause confusion upon resource release
  {
    std::unique_ptr<IRHandler> global;
    auto gh = std::find_if(handlers_.begin(), handlers_.end(), [](auto& handler) {
        return handler->name() == GLOBAL_SCOPE_INIT_NAME;
    });
    if (gh != handlers_.end()) {
      global = std::move(*gh);
      handlers_.erase(gh);
    }
    handlers_.clear();
    global.reset(nullptr);
  }

  constantArrays_.clear();
  numbers_.clear();
  strings_.clear();
  ipaddrs_.clear();
  cidrs_.clear();
  builtinHandlers_.clear();
  builtinFunctions_.clear();
}

void IRProgram::dump() {
  printf("; IRProgram\n");

  for (auto& handler : handlers_)
    handler->dump();
}

IRHandler* IRProgram::createHandler(const std::string& name) {
  handlers_.emplace_back(std::make_unique<IRHandler>(name, this));
  return handlers_.back().get();
}

template <typename T, typename U>
T* IRProgram::get(std::vector<std::unique_ptr<T>>& table, const U& literal) {
  for (size_t i = 0, e = table.size(); i != e; ++i)
    if (table[i]->get() == literal)
      return table[i].get();

  table.emplace_back(std::make_unique<T>(literal));
  return table.back().get();
}

template <typename T, typename U>
T* IRProgram::get(std::vector<T>& table, const U& literal) {
  for (size_t i = 0, e = table.size(); i != e; ++i)
    if (table[i].get() == literal)
      return &table[i];

  table.emplace_back(literal);
  return &table.back();
}

template ConstantInt* IRProgram::get<ConstantInt, int64_t>(
    std::vector<std::unique_ptr<ConstantInt>>&, const int64_t&);

template ConstantArray* IRProgram::get<
    ConstantArray, std::vector<Constant*>>(
        std::vector<ConstantArray>&, const std::vector<Constant*>&);

template ConstantString* IRProgram::get<ConstantString, std::string>(
    std::vector<std::unique_ptr<ConstantString>>&, const std::string&);

template ConstantIP* IRProgram::get<ConstantIP, util::IPAddress>(
    std::vector<std::unique_ptr<ConstantIP>>&, const util::IPAddress&);

template ConstantCidr* IRProgram::get<ConstantCidr, util::Cidr>(
    std::vector<std::unique_ptr<ConstantCidr>>&, const util::Cidr&);

template ConstantRegExp* IRProgram::get<ConstantRegExp, util::RegExp>(
    std::vector<std::unique_ptr<ConstantRegExp>>&, const util::RegExp&);

}  // namespace flow
