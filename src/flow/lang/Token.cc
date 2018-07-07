// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/lang/Token.h>
#include <flow/util/assert.h>

namespace flow::lang {

bool TokenTraits::isKeyword(Token t) {
  switch (t) {
    case Token::Var:
    case Token::On:
    case Token::While:
    case Token::For:
    case Token::Do:
    case Token::If:
    case Token::Match:
    case Token::Then:
    case Token::Else:
    case Token::Unless:
    case Token::Import:
    case Token::From:
    case Token::Handler:
      return true;
    default:
      return false;
  }
}

bool TokenTraits::isReserved(Token t) {
  return false;
}

bool TokenTraits::isType(Token t) {
  switch (t) {
    case Token::VoidType:
    case Token::BoolType:
    case Token::NumberType:
    case Token::StringType:
      return true;
    default:
      return false;
  }
}

bool TokenTraits::isOperator(Token t) {
  switch (t) {
    case Token::Assign:
    case Token::Question:
    case Token::And:
    case Token::Or:
    case Token::Xor:
    case Token::Equal:
    case Token::UnEqual:
    case Token::Less:
    case Token::Greater:
    case Token::LessOrEqual:
    case Token::GreaterOrEqual:
    case Token::PrefixMatch:
    case Token::SuffixMatch:
    case Token::RegexMatch:
    case Token::HashRocket:
    case Token::Plus:
    case Token::Minus:
    case Token::Mul:
    case Token::Div:
    case Token::Shl:
    case Token::Shr:
    case Token::Comma:
    case Token::Pow:
      return true;
    default:
      return false;
  }
}

bool TokenTraits::isUnaryOp(Token t) {
  // token Plus and Minus can be both, unary and binary
  switch (t) {
    //case Token::Plus:
    case Token::Minus:
    case Token::Not:
    case Token::BitNot:
      return true;
    default:
      return false;
  }
}

bool TokenTraits::isPrimaryOp(Token t) { return false; }

bool TokenTraits::isRelOp(Token t) {
  switch (t) {
    case Token::Equal:
    case Token::UnEqual:
    case Token::Less:
    case Token::Greater:
    case Token::LessOrEqual:
    case Token::GreaterOrEqual:
    case Token::PrefixMatch:
    case Token::SuffixMatch:
    case Token::RegexMatch:
      return true;
    default:
      return false;
  }
}

bool TokenTraits::isLiteral(Token t) {
  switch (t) {
    case Token::InterpolatedStringFragment:
    /*
     * skip Token::InterpolatedStringEnd,
     * because it's a tail of a composed literal that is
     * going to be explicitely matched when needed.
     */
    case Token::Boolean:
    case Token::Number:
    case Token::String:
    case Token::RawString:
    case Token::RegExp:
    case Token::IP:
    case Token::Cidr:
    case Token::NamedParam:
      return true;
    default:
      return false;
  }
}

std::string to_string(Token t) {
  switch (t) {
    case Token::Unknown:
      return "Unknown";
    case Token::Boolean:
      return "Boolean";
    case Token::Number:
      return "Number";
    case Token::String:
      return "String";
    case Token::RawString:
      return "RawString";
    case Token::RegExp:
      return "RegExp";
    case Token::IP:
      return "IP";
    case Token::Cidr:
      return "CIDR";
    case Token::Assign:
      return "=";
    case Token::OrAssign:
      return "|=";
    case Token::AndAssign:
      return "&=";
    case Token::PlusAssign:
      return "+=";
    case Token::MinusAssign:
      return "-=";
    case Token::MulAssign:
      return "*=";
    case Token::DivAssign:
      return "/=";
    case Token::Semicolon:
      return ";";
    case Token::Question:
      return "?";
    case Token::Colon:
      return ":";
    case Token::And:
      return "and";
    case Token::Or:
      return "or";
    case Token::Xor:
      return "xor";
    case Token::Equal:
      return "==";
    case Token::UnEqual:
      return "!=";
    case Token::Less:
      return "<";
    case Token::Greater:
      return ">";
    case Token::LessOrEqual:
      return "<=";
    case Token::GreaterOrEqual:
      return ">=";
    case Token::PrefixMatch:
      return "=^";
    case Token::SuffixMatch:
      return "=$";
    case Token::RegexMatch:
      return "=~";
    case Token::HashRocket:
      return "=>";
    case Token::In:
      return "in";
    case Token::Plus:
      return "+";
    case Token::Minus:
      return "-";
    case Token::Mul:
      return "*";
    case Token::Div:
      return "/";
    case Token::Mod:
      return "%";
    case Token::Shl:
      return "shl";
    case Token::Shr:
      return "shr";
    case Token::Comma:
      return ",";
    case Token::Pow:
      return "**";
    case Token::Not:
      return "not";
    case Token::BitNot:
      return "~";
    case Token::BitOr:
      return "|";
    case Token::BitAnd:
      return "&";
    case Token::BitXor:
      return "^";
    case Token::BrOpen:
      return "[";
    case Token::BrClose:
      return "]";
    case Token::RndOpen:
      return "(";
    case Token::RndClose:
      return ")";
    case Token::Begin:
      return "{";
    case Token::End:
      return "}";
    case Token::Var:
      return "var";
    case Token::Do:
      return "do";
    case Token::If:
      return "if";
    case Token::Then:
      return "then";
    case Token::Else:
      return "else";
    case Token::Unless:
      return "unless";
    case Token::Match:
      return "match";
    case Token::On:
      return "on";
    case Token::While:
      return "while";
    case Token::For:
      return "for";
    case Token::Import:
      return "import";
    case Token::From:
      return "from";
    case Token::Handler:
      return "handler";
    case Token::VoidType:
      return "void()";
    case Token::BoolType:
      return "bool()";
    case Token::NumberType:
      return "int()";
    case Token::StringType:
      return "string()";
    case Token::Ident:
      return "Ident";
    case Token::RegExpGroup:
      return "RegExpGroup";
    case Token::Period:
      return "Period";
    case Token::DblPeriod:
      return "DblPeriod";
    case Token::Ellipsis:
      return "Ellipsis";
    case Token::Comment:
      return "Comment";
    case Token::Eof:
      return "EOF";
    case Token::NamedParam:
      return "NamedParam";
    case Token::InterpolatedStringFragment:
      return "InterpolatedStringFragment";
    case Token::InterpolatedStringEnd:
      return "InterpolatedStringEnd";
    default:
      FLOW_ASSERT(false, "FIXME: Invalid Token.");
  }
}

}  // namespace flow
