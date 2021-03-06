// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/ir/Constant.h>
#include <flow/Signature.h>
#include <flow/NativeCallback.h>

#include <string>
#include <vector>
#include <list>

namespace flow {

class IRBuiltinHandler : public Constant {
 public:
  explicit IRBuiltinHandler(const NativeCallback& cb)
      : Constant(LiteralType::Boolean, cb.signature().name()),
        native_(cb) {}

  const Signature& signature() const { return native_.signature(); }
  const NativeCallback& getNative() const { return native_; }

 private:
  const NativeCallback& native_;
};

}  // namespace flow
