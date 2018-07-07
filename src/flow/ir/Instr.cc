// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/Instr.h>
#include <flow/ir/ConstantValue.h>
#include <flow/ir/ConstantArray.h>
#include <flow/ir/IRBuiltinHandler.h>
#include <flow/ir/IRBuiltinFunction.h>
#include <flow/ir/BasicBlock.h>

#include <fmt/format.h>

#include <cassert>
#include <cinttypes>
#include <sstream>
#include <utility>

namespace flow {

Instr::Instr(const Instr& v)
    : Value(v), basicBlock_(nullptr), operands_(v.operands_) {
  for (Value* op : operands_) {
    if (op) {
      op->addUse(this);
    }
  }
}

Instr::Instr(LiteralType ty, const std::vector<Value*>& ops,
             const std::string& name)
    : Value(ty, name), basicBlock_(nullptr), operands_(ops) {
  for (Value* op : operands_) {
    if (op) {
      op->addUse(this);
    }
  }
}

Instr::~Instr() {
  for (Value* op : operands_) {
    if (op != nullptr) {
      // remove this instruction as user of that operand
      op->removeUse(this);

      // if operand is a BasicBlock, unlink it as successor
      if (BasicBlock* parent = getBasicBlock(); parent != nullptr) {
        if (BasicBlock* oldBB = dynamic_cast<BasicBlock*>(op)) {
          parent->unlinkSuccessor(oldBB);
        }
      }
    }
  }
}

void Instr::addOperand(Value* value) {
  operands_.push_back(value);

  value->addUse(this);

  if (BasicBlock* newBB = dynamic_cast<BasicBlock*>(value)) {
    getBasicBlock()->linkSuccessor(newBB);
  }
}

Value* Instr::setOperand(size_t i, Value* value) {
  Value* old = operands_[i];
  operands_[i] = value;

  if (old) {
    old->removeUse(this);

    if (BasicBlock* oldBB = dynamic_cast<BasicBlock*>(old)) {
      getBasicBlock()->unlinkSuccessor(oldBB);
    }
  }

  if (value) {
    value->addUse(this);

    if (BasicBlock* newBB = dynamic_cast<BasicBlock*>(value)) {
      getBasicBlock()->linkSuccessor(newBB);
    }
  }

  return old;
}

size_t Instr::replaceOperand(Value* old, Value* replacement) {
  size_t count = 0;

  for (size_t i = 0, e = operands_.size(); i != e; ++i) {
    if (operands_[i] == old) {
      setOperand(i, replacement);
      ++count;
    }
  }

  return count;
}

void Instr::clearOperands() {
  for (size_t i = 0, e = operands_.size(); i != e; ++i) {
    setOperand(i, nullptr);
  }

  operands_.clear();
}

std::unique_ptr<Instr> Instr::replace(std::unique_ptr<Instr> newInstr) {
  if (basicBlock_) {
    return basicBlock_->replace(this, std::move(newInstr));
  } else {
    return nullptr;
  }
}

void Instr::dumpOne(const char* mnemonic) {
  fmt::print("\t{}\n", formatOne(mnemonic));
}

std::string Instr::formatOne(std::string mnemonic) const {
  std::stringstream sstr;

  if (type() == LiteralType::Void)
    sstr << mnemonic;
  else if (name().empty())
    sstr << fmt::format("%??? = {}", mnemonic);
  else
    sstr << fmt::format("%{} = {}", name(), mnemonic);

  for (size_t i = 0, e = operands_.size(); i != e; ++i) {
    sstr << (i ? ", " : " ");
    Value* arg = operands_[i];
    if (dynamic_cast<Constant*>(arg)) {
      if (auto i = dynamic_cast<ConstantInt*>(arg)) {
        sstr << i->get();
        continue;
      }
      if (auto s = dynamic_cast<ConstantString*>(arg)) {
        sstr << '"' << s->get() << '"';
        continue;
      }
      if (auto ip = dynamic_cast<ConstantIP*>(arg)) {
        sstr << ip->get().c_str();
        continue;
      }
      if (auto cidr = dynamic_cast<ConstantCidr*>(arg)) {
        sstr << cidr->get().str();
        continue;
      }
      if (auto re = dynamic_cast<ConstantRegExp*>(arg)) {
        sstr << '/' << re->get().pattern() << '/';
        continue;
      }
      if (auto bh = dynamic_cast<IRBuiltinHandler*>(arg)) {
        sstr << bh->signature().to_s();
        continue;
      }
      if (auto bf = dynamic_cast<IRBuiltinFunction*>(arg)) {
        sstr << bf->signature().to_s();
        continue;
      }
      if (auto ar = dynamic_cast<ConstantArray*>(arg)) {
        sstr << '[';
        size_t i = 0;
        switch (ar->type()) {
          case LiteralType::IntArray:
            for (const auto& v : ar->get()) {
              if (i) sstr << ", ";
              sstr << static_cast<ConstantInt*>(v)->get();
              ++i;
            }
            break;
          case LiteralType::StringArray:
            for (const auto& v : ar->get()) {
              if (i) sstr << ", ";
              sstr << '"' << static_cast<ConstantString*>(v)->get() << '"';
              ++i;
            }
            break;
          case LiteralType::IPAddrArray:
            for (const auto& v : ar->get()) {
              if (i) sstr << ", ";
              sstr << static_cast<ConstantIP*>(v)->get().str();
              ++i;
            }
            break;
          case LiteralType::CidrArray:
            for (const auto& v : ar->get()) {
              if (i) sstr << ", ";
              sstr << static_cast<ConstantCidr*>(v)->get().str();
              ++i;
            }
            break;
          default:
            abort();
        }
        sstr << ']';
        continue;
      }
    }
    sstr << '%' << arg->name();
  }

  // XXX sometimes u're interested in the name of the instr, even though it
  // doesn't yield a result value on the stack
  // if (type() == LiteralType::Void) {
  //   sstr << "\t; (%" << name() << ")";
  // }

  return sstr.str();
}

}  // namespace flow
