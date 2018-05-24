// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <utility>
#include <memory>
#include <iosfwd>
#include <cstdint>
#include <fmt/format.h>

namespace flow::lang {

//! \addtogroup Flow
//@{

enum class Token {
  Unknown,

  // literals
  Boolean,
  Number,
  String,
  RawString,
  RegExp,
  IP,
  Cidr,
  NamedParam,
  InterpolatedStringFragment,  // "hello #{" or "} world #{"
  InterpolatedStringEnd,       // "} end"

  // symbols
  Assign,
  OrAssign,
  AndAssign,
  PlusAssign,
  MinusAssign,
  MulAssign,
  DivAssign,
  Semicolon,
  Question,
  Colon,
  And,
  Or,
  Xor,
  Equal,
  UnEqual,
  Less,
  Greater,
  LessOrEqual,
  GreaterOrEqual,
  PrefixMatch,
  SuffixMatch,
  RegexMatch,
  In,
  HashRocket,
  Plus,
  Minus,
  Mul,
  Div,
  Mod,
  Shl,
  Shr,
  Comma,
  Pow,
  Not,
  BitNot,
  BitOr,
  BitAnd,
  BitXor,
  BrOpen,
  BrClose,
  RndOpen,
  RndClose,
  Begin,
  End,

  // keywords
  Var,
  Do,
  Handler,
  If,
  Then,
  Else,
  Unless,
  Match,
  On,
  While,
  For,
  Import,
  From,

  // data types
  VoidType,
  BoolType,
  NumberType,
  StringType,

  // misc
  Ident,
  RegExpGroup,
  Period,
  DblPeriod,
  Ellipsis,
  Comment,
  Eof,
  COUNT
};

class TokenTraits {
 public:
  static bool isKeyword(Token t);
  static bool isReserved(Token t);
  static bool isLiteral(Token t);
  static bool isType(Token t);

  static bool isOperator(Token t);
  static bool isUnaryOp(Token t);
  static bool isPrimaryOp(Token t);
  static bool isRelOp(Token t);
};

std::string to_string(Token t);

//!@}

}  // namespace flow::lang

namespace std {
  template <>
  struct hash<flow::lang::Token> {
    uint32_t operator()(flow::lang::Token v) const {
      return static_cast<uint32_t>(v);
    }
  };
}

namespace fmt {
  template<>
  struct formatter<flow::lang::Token> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const flow::lang::Token& v, FormatContext &ctx) {
      return format_to(ctx.begin(), to_string(v));
    }
  };
}
