// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/NativeCallback.h>
#include <flow/lang/AST.h>
#include <flow/lang/Lexer.h>
#include <flow/lang/Parser.h>
#include <flow/vm/Runtime.h>
#include <unordered_map>
#include <memory>

namespace flow::lang {
  enum class OpSig {
    Invalid,
    BoolBool,
    NumNum,
    StringString,
    StringRegexp,
    IpIp,
    IpCidr,
    CidrCidr,
  };
}

namespace std {
  template <>
  struct hash<flow::lang::OpSig> {
    uint32_t operator()(flow::lang::OpSig v) const noexcept {
      return static_cast<uint32_t>(v);
    }
  };
}

namespace flow::lang {

// {{{ scoped(SCOPED_SYMBOL)
class Parser::Scope {
 private:
  Parser* parser_;
  SymbolTable* table_;
  bool flipped_;

 public:
  Scope(Parser* parser, SymbolTable* table)
      : parser_(parser), table_(table), flipped_(false) {
    parser_->enterScope(table_);
  }

  Scope(Parser* parser, ScopedSym* symbol)
      : parser_(parser), table_(symbol->scope()), flipped_(false) {
    parser_->enterScope(table_);
  }

  Scope(Parser* parser, std::unique_ptr<SymbolTable>& table)
      : parser_(parser), table_(table.get()), flipped_(false) {
    parser_->enterScope(table_);
  }

  template <typename T>
  Scope(Parser* parser, std::unique_ptr<T>& symbol)
      : parser_(parser), table_(symbol->scope()), flipped_(false) {
    parser_->enterScope(table_);
  }

  bool flip() {
    flipped_ = !flipped_;
    return flipped_;
  }

  Scope(const Scope&) = delete;
  Scope(Scope&&) = delete;
  Scope& operator=(const Scope&) = delete;
  Scope& operator=(Scope&&) = delete;

  ~Scope() { parser_->leaveScope(); }
};
#define scoped(SCOPED_SYMBOL) \
  for (Parser::Scope _(this, (SCOPED_SYMBOL)); _.flip();)
// }}}

Parser::Parser(std::set<Feature> features,
               diagnostics::Report* report,
               Runtime* runtime,
               ImportHandler importHandler)
    : features_{std::move(features)},
      report_{*report},
      lexer_{std::make_unique<Lexer>(report)},
      scopeStack_{nullptr},
      runtime_{runtime},
      importHandler_{std::move(importHandler)} {
  // enterScope("global");
}

Parser::~Parser() {
  // leaveScope();
  // assert(scopeStack_ == nullptr && "scopeStack not properly unwind. probably
  // a bug.");
}

void Parser::openString(const std::string& content) {
  lexer_->openString(content);
}

void Parser::openLocalFile(const std::string& filename) {
  lexer_->openLocalFile(filename);
}

void Parser::openStream(std::unique_ptr<std::istream>&& ifs,
                            const std::string& filename) {
  lexer_->openStream(std::move(ifs), filename);
}

std::unique_ptr<SymbolTable> Parser::enterScope(const std::string& title) {
  // printf("Parser::enterScope(): new top: %p \"%s\" (outer: %p \"%s\")\n", scope,
  // scope->name().c_str(), scopeStack_, scopeStack_ ?
  // scopeStack_->name().c_str() : "");

  auto st = std::make_unique<SymbolTable>(currentScope(), title);
  st->setOuterTable(scopeStack_);
  scopeStack_ = st.get();
  return st;
}

SymbolTable* Parser::enterScope(SymbolTable* scope) {
  // printf("Parser::enterScope(): new top: %p \"%s\" (outer: %p \"%s\")\n", scope,
  // scope->name().c_str(), scopeStack_, scopeStack_ ?
  // scopeStack_->name().c_str() : "");

  scope->setOuterTable(scopeStack_);
  scopeStack_ = scope;

  return scope;
}

SymbolTable* Parser::globalScope() const {
  if (SymbolTable* st = scopeStack_; st != nullptr) {
    while (st->outerTable()) {
      st = st->outerTable();
    }
    return st;
  }
  return nullptr;
}

SymbolTable* Parser::leaveScope() {
  SymbolTable* popped = scopeStack_;
  scopeStack_ = scopeStack_->outerTable();

  // printf("Parser::leaveScope(): new top: %p \"%s\" (outer: %p \"%s\")\n", popped,
  // popped->name().c_str(), scopeStack_, scopeStack_ ?
  // scopeStack_->name().c_str() : "");

  return popped;
}

std::unique_ptr<UnitSym> Parser::parse() {
  return unit();
}

// {{{ type system
/**
 * Computes VM opcode for binary operation.
 *
 * @param token operator between left and right expression
 * @param left left hand expression
 * @param right right hand expression
 *
 * @return opcode that matches given expressions operator or EXIT if operands
 *incompatible.
 */
Opcode makeOperator(Token token, Expr* left, Expr* right) {
  // (bool, bool)     == !=
  // (num, num)       + - * / % ** << >> & | ^ and or xor == != <= >= < >
  // (string, string) + == != <= >= < > =^ =$ in
  // (string, regex)  =~
  // (ip, ip)         == !=
  // (ip, cidr)       in
  // (cidr, cidr)     == != in

  auto isString = [](LiteralType t) { return t == LiteralType::String; };
  auto isNumber = [](LiteralType t) { return t == LiteralType::Number; };
  auto isBool = [](LiteralType t) { return t == LiteralType::Boolean; };
  auto isIPAddr = [](LiteralType t) { return t == LiteralType::IPAddress; };
  auto isCidr = [](LiteralType t) { return t == LiteralType::Cidr; };
  auto isRegExp = [](LiteralType t) { return t == LiteralType::RegExp; };

  LiteralType leftType = left->getType();
  LiteralType rightType = right->getType();

  OpSig opsig = OpSig::Invalid;
  if (isBool(leftType) && isBool(rightType))
    opsig = OpSig::BoolBool;
  else if (isNumber(leftType) && isNumber(rightType))
    opsig = OpSig::NumNum;
  else if (isString(leftType) && isString(rightType))
    opsig = OpSig::StringString;
  else if (isString(leftType) && isRegExp(rightType))
    opsig = OpSig::StringRegexp;
  else if (isIPAddr(leftType) && isIPAddr(rightType))
    opsig = OpSig::IpIp;
  else if (isIPAddr(leftType) && isCidr(rightType))
    opsig = OpSig::IpCidr;
  else if (isCidr(leftType) && isCidr(rightType))
    opsig = OpSig::CidrCidr;

  static const std::unordered_map<OpSig, std::unordered_map<Token, Opcode>>
  ops = {{OpSig::BoolBool,
          {{Token::Equal, Opcode::NCMPEQ},
           {Token::UnEqual, Opcode::NCMPNE},
           {Token::And, Opcode::BAND},
           {Token::Or, Opcode::BOR},
           {Token::Xor, Opcode::BXOR}, }},
         {OpSig::NumNum,
          {{Token::Plus, Opcode::NADD},
           {Token::Minus, Opcode::NSUB},
           {Token::Mul, Opcode::NMUL},
           {Token::Div, Opcode::NDIV},
           {Token::Mod, Opcode::NREM},
           {Token::Pow, Opcode::NPOW},
           {Token::Shl, Opcode::NSHL},
           {Token::Shr, Opcode::NSHR},
           {Token::BitAnd, Opcode::NAND},
           {Token::BitOr, Opcode::NOR},
           {Token::BitXor, Opcode::NXOR},
           {Token::Equal, Opcode::NCMPEQ},
           {Token::UnEqual, Opcode::NCMPNE},
           {Token::LessOrEqual, Opcode::NCMPLE},
           {Token::GreaterOrEqual, Opcode::NCMPGE},
           {Token::Less, Opcode::NCMPLT},
           {Token::Greater, Opcode::NCMPGT}, }},
         {OpSig::StringString,
          {{Token::Plus, Opcode::SADD},
           {Token::Equal, Opcode::SCMPEQ},
           {Token::UnEqual, Opcode::SCMPNE},
           {Token::LessOrEqual, Opcode::SCMPLE},
           {Token::GreaterOrEqual, Opcode::SCMPGE},
           {Token::Less, Opcode::SCMPLT},
           {Token::Greater, Opcode::SCMPGT},
           {Token::PrefixMatch, Opcode::SCMPBEG},
           {Token::SuffixMatch, Opcode::SCMPEND},
           {Token::In, Opcode::SCONTAINS}, }},
         {OpSig::StringRegexp, {{Token::RegexMatch, Opcode::SREGMATCH}, }},
         {OpSig::IpIp,
          {{Token::Equal, Opcode::PCMPEQ},
           {Token::UnEqual, Opcode::PCMPNE}, }},
         {OpSig::IpCidr, {{Token::In, Opcode::PINCIDR}, }},
         {OpSig::CidrCidr,
          {{Token::Equal, Opcode::NOP},    // TODO
           {Token::UnEqual, Opcode::NOP},  // TODO
           {Token::In, Opcode::NOP},       // TODO
          }}, };

  auto a = ops.find(opsig);
  if (a == ops.end()) return Opcode::EXIT;

  auto b = a->second.find(token);
  if (b == a->second.end()) return Opcode::EXIT;

  return b->second;
}

/**
 * Casts given epxression into an expression of fixed type.
 *
 * @param source source expression to cast
 * @param target target (token) type to cast to.
 */
Opcode makeOperator(Token target, Expr* source) {
  static const std::unordered_map<LiteralType,
                                  std::unordered_map<Token, Opcode>> ops =
      {{LiteralType::Number,
        {{Token::Not, Opcode::NCMPZ},
         {Token::BitNot, Opcode::NNOT},
         {Token::Minus, Opcode::NNEG},
         {Token::StringType, Opcode::N2S},
         {Token::BoolType, Opcode::NCMPZ},
         {Token::NumberType, Opcode::NOP}, }},
       {LiteralType::Boolean,
        {{Token::Not, Opcode::BNOT},
         {Token::BoolType, Opcode::NOP},
         {Token::StringType, Opcode::N2S},  // XXX or better print "true" | "false" ?
        }},
       {LiteralType::String,
        {{Token::Not, Opcode::SISEMPTY},
         {Token::NumberType, Opcode::S2N},
         {Token::StringType, Opcode::NOP}, }},
       {LiteralType::IPAddress, {{Token::StringType, Opcode::P2S}, }},
       {LiteralType::Cidr, {{Token::StringType, Opcode::C2S}, }},
       {LiteralType::RegExp, {{Token::StringType, Opcode::R2S}, }}, };

  LiteralType sourceType = source->getType();

  auto a = ops.find(sourceType);
  if (a == ops.end()) return Opcode::EXIT;

  auto b = a->second.find(target);
  if (b == a->second.end()) return Opcode::EXIT;

  return b->second;
}
// }}}
// {{{ lexing
Token Parser::nextToken() const { return lexer_->nextToken(); }

bool Parser::expect(Token value) {
  if (token() != value) {
    report_.syntaxError(lastLocation(),
                        "Unexpected token '{}' (expected: '{}')",
                        token(), value);
    return false;
  }
  return true;
}

bool Parser::consume(Token value) {
  if (!expect(value)) return false;

  nextToken();
  return true;
}

bool Parser::consumeIf(Token value) {
  if (token() == value) {
    nextToken();
    return true;
  } else {
    return false;
  }
}

bool Parser::consumeUntil(Token value) {
  for (;;) {
    if (token() == value) {
      nextToken();
      return true;
    }

    if (token() == Token::Eof) return false;

    nextToken();
  }
}
// }}}
// {{{ decls
std::unique_ptr<UnitSym> Parser::unit() {
  auto unit = std::make_unique<UnitSym>();

  scoped(unit) {
    importRuntime();

    while (token() == Token::Import) {
      if (!importDecl(unit.get())) {
        return nullptr;
      }
    }

    while (std::unique_ptr<Symbol> symbol = decl()) {
      if (!dynamic_cast<VariableSym*>(symbol.get())) {
        currentScope()->appendSymbol(std::move(symbol));
      } else if (hasFeature(Feature::GlobalScope)) {
        currentScope()->appendSymbol(std::move(symbol));
      } else {
        report_.syntaxError(symbol->location(), "Global-scope feature not enabled.");
      }
    }
  }

  return unit;
}

void Parser::importRuntime() {
  if (runtime_) {
    for (const auto& builtin : runtime_->builtins()) {
      declareBuiltin(builtin);
    }
  }
}

void Parser::declareBuiltin(const NativeCallback* native) {
  if (native->isHandler()) {
    createSymbol<BuiltinHandlerSym>(*native);
  } else {
    createSymbol<BuiltinFunctionSym>(*native);
  }
}

std::unique_ptr<Symbol> Parser::decl() {
  switch (token()) {
    case Token::Var:
      return varDecl();
    case Token::Handler:
      return handlerDecl(true);
    case Token::Ident:
      return handlerDecl(false);
    default:
      return nullptr;
  }
}

// 'var' IDENT ['=' EXPR] ';'
std::unique_ptr<VariableSym> Parser::varDecl() {
  SourceLocation loc(lexer_->location());

  if (!consume(Token::Var)) return nullptr;

  if (!consume(Token::Ident)) return nullptr;

  std::string name = stringValue();

  if (!consume(Token::Assign)) return nullptr;

  std::unique_ptr<Expr> initializer = expr();
  if (!initializer) return nullptr;

  loc.update(initializer->location().end);
  consume(Token::Semicolon);

  return std::make_unique<VariableSym>(name, std::move(initializer), loc);
}

bool Parser::importDecl(UnitSym* unit) {
  // 'import' NAME_OR_NAMELIST ['from' PATH] ';'
  nextToken();  // skip 'import'

  std::list<std::string> names;
  if (!importOne(names)) {
    consumeUntil(Token::Semicolon);
    return false;
  }

  while (token() == Token::Comma) {
    nextToken();
    if (!importOne(names)) {
      consumeUntil(Token::Semicolon);
      return false;
    }
  }

  std::string path;
  if (consumeIf(Token::From)) {
    path = stringValue();

    if (!consumeOne(Token::String, Token::RawString)) {
      consumeUntil(Token::Semicolon);
      return false;
    }

    if (!path.empty() && path[0] != '/') {
      std::string base(lexer_->location().filename);

      size_t r = base.rfind('/');
      if (r != std::string::npos) ++r;

      base = base.substr(0, r);
      path = base + path;
    }
  }

  for (const std::string& name: names) {
    std::vector<NativeCallback*> builtins;

    if (importHandler_ && !importHandler_(name, path, &builtins))
      return false;

    unit->import(name, path);

    for (NativeCallback* native : builtins) {
      declareBuiltin(native);
    }
  }

  consume(Token::Semicolon);
  return true;
}

bool Parser::importOne(std::list<std::string>& names) {
  switch (token()) {
    case Token::Ident:
    case Token::String:
    case Token::RawString:
      names.push_back(stringValue());
      nextToken();
      break;
    case Token::RndOpen:
      nextToken();
      if (!importOne(names)) return false;

      while (token() == Token::Comma) {
        nextToken();  // skip comma
        if (!importOne(names)) return false;
      }

      if (!consume(Token::RndClose)) return false;
      break;
    default:
      report_.syntaxError(lastLocation(), "Syntax error in import declaration. Unexpected token {}.", token());
      return false;
  }
  return true;
}

// handlerDecl ::= 'handler' IDENT (';' | [do] stmt)
std::unique_ptr<HandlerSym> Parser::handlerDecl(bool keyword) {
  SourceLocation loc(location());

  if (keyword) {
    nextToken();  // 'handler'
  }

  consume(Token::Ident);
  std::string name = stringValue();
  if (consumeIf(Token::Semicolon)) {  // forward-declaration
    loc.update(end());
    return std::make_unique<HandlerSym>(name, loc);
  }

  std::unique_ptr<SymbolTable> st = enterScope("handler-" + name);
  std::unique_ptr<Stmt> body = stmt();
  leaveScope();

  if (!body) return nullptr;

  loc.update(body->location().end);

  // forward-declared / previousely -declared?
  if (HandlerSym* handler = currentScope()->lookup<HandlerSym>(name, Lookup::Self)) {
    if (handler->body() != nullptr) {
      // TODO say where we found the other hand, compared to this one.
      report_.typeError(lastLocation(), "Redeclaring handler \"{}\"", handler->name());
      return nullptr;
    }
    handler->implement(std::move(st), std::move(body));
    handler->owner()->removeSymbol(handler);
    return std::unique_ptr<HandlerSym>(handler);
  }

  return std::make_unique<HandlerSym>(name, std::move(st), std::move(body), loc);
}
// }}}
// {{{ expr
std::unique_ptr<Expr> Parser::expr() {
  return logicExpr();
}

std::unique_ptr<Expr> Parser::logicExpr() {
  std::unique_ptr<Expr> lhs = notExpr();
  if (!lhs) return nullptr;

  for (;;) {
    switch (token()) {
      case Token::And:
      case Token::Xor:
      case Token::Or: {
        Token binop = token();
        nextToken();

        std::unique_ptr<Expr> rhs = notExpr();
        if (!rhs) return nullptr;

        Opcode opc = makeOperator(binop, lhs.get(), rhs.get());
        if (opc == Opcode::EXIT) {
          report_.typeError(
              lastLocation(),
              "Incompatible binary expression operands ({} {} {}).",
              lhs->getType(), binop, rhs->getType());
          return nullptr;
        }

        lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
        break;
      }
      default:
        return lhs;
    }
  }
}

std::unique_ptr<Expr> Parser::notExpr() {
  size_t nots = 0;

  SourceLocation loc = location();

  while (consumeIf(Token::Not)) nots++;

  std::unique_ptr<Expr> subExpr = relExpr();
  if (!subExpr) return nullptr;

  if ((nots % 2) == 0) return subExpr;

  Opcode op = makeOperator(Token::Not, subExpr.get());
  if (op == Opcode::EXIT) {
    report_.typeError(
        lastLocation(),
        "Type cast error in unary 'not'-operator. Invalid source type <{}>.",
        subExpr->getType());
    return nullptr;
  }

  return std::make_unique<UnaryExpr>(op, std::move(subExpr), loc.update(end()));
}

std::unique_ptr<Expr> Parser::relExpr() {
  std::unique_ptr<Expr> lhs = addExpr();
  if (!lhs)
    return nullptr;

  switch (token()) {
    case Token::Equal:
    case Token::UnEqual:
    case Token::Less:
    case Token::Greater:
    case Token::LessOrEqual:
    case Token::GreaterOrEqual:
    case Token::PrefixMatch:
    case Token::SuffixMatch:
    case Token::RegexMatch:
    case Token::In: {
      Token binop = token();
      nextToken();

      std::unique_ptr<Expr> rhs = addExpr();
      if (!rhs) return nullptr;

      Opcode opc = makeOperator(binop, lhs.get(), rhs.get());
      if (opc == Opcode::EXIT) {
        report_.typeError(lastLocation(),
                          "Incompatible binary expression ({} in {}).",
                          lhs->getType(), rhs->getType());
        return nullptr;
      }

      lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
      return lhs;
    }
    default:
      return lhs;
  }
}

std::unique_ptr<Expr> Parser::addExpr() {
  std::unique_ptr<Expr> lhs = mulExpr();
  if (!lhs) return nullptr;

  for (;;) {
    switch (token()) {
      case Token::Plus:
      case Token::Minus: {
        Token binop = token();
        nextToken();

        std::unique_ptr<Expr> rhs = mulExpr();
        if (!rhs) return nullptr;

        Opcode opc = makeOperator(binop, lhs.get(), rhs.get());
        if (opc == Opcode::EXIT) {
          report_.typeError(
              lastLocation(),
              "Incompatible binary expression operands ({} {} {}).",
              lhs->getType(), binop, rhs->getType());
          return nullptr;
        }

        lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
        break;
      }
      default:
        return lhs;
    }
  }
}

std::unique_ptr<Expr> Parser::mulExpr() {
  std::unique_ptr<Expr> lhs = powExpr();
  if (!lhs) return nullptr;

  for (;;) {
    switch (token()) {
      case Token::Mul:
      case Token::Div:
      case Token::Mod:
      case Token::Shl:
      case Token::Shr: {
        Token binop = token();
        nextToken();

        std::unique_ptr<Expr> rhs = powExpr();
        if (!rhs) return nullptr;

        Opcode opc = makeOperator(binop, lhs.get(), rhs.get());
        if (opc == Opcode::EXIT) {
          report_.typeError(
              lastLocation(),
              "Incompatible binary expression operands ({} {} {}).",
              lhs->getType(), binop, rhs->getType());
          return nullptr;
        }

        lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
        break;
      }
      default:
        return lhs;
    }
  }
}

std::unique_ptr<Expr> Parser::powExpr() {
  // powExpr ::= negExpr ('**' powExpr)*
  SourceLocation sloc(location());
  std::unique_ptr<Expr> lhs = negExpr();
  if (!lhs) return nullptr;

  while (token() == Token::Pow) {
    nextToken();

    std::unique_ptr<Expr> rhs = powExpr();
    if (!rhs) return nullptr;

    auto opc = makeOperator(Token::Pow, lhs.get(), rhs.get());
    if (opc == Opcode::EXIT) {
      report_.typeError(
          lastLocation(),
          "Incompatible binary expression operands ({} {} {}).",
          lhs->getType(), Token::Pow, rhs->getType());
      return nullptr;
    }

    lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

std::unique_ptr<Expr> Parser::negExpr() {
  // negExpr ::= ['-'] primaryExpr
  SourceLocation loc = location();

  if (consumeIf(Token::Minus)) {
    std::unique_ptr<Expr> e = negExpr();
    if (!e) return nullptr;

    Opcode op = makeOperator(Token::Minus, e.get());
    if (op == Opcode::EXIT) {
      report_.typeError(
          lastLocation(),
          "Type cast error in unary 'neg'-operator. Invalid source type <{}>.",
          e->getType());
      return nullptr;
    }

    e = std::make_unique<UnaryExpr>(op, std::move(e), loc.update(end()));
    return e;
  } else {
    return bitNotExpr();
  }
}

std::unique_ptr<Expr> Parser::bitNotExpr() {
  // negExpr ::= ['~'] primaryExpr
  SourceLocation loc = location();

  if (consumeIf(Token::BitNot)) {
    std::unique_ptr<Expr> e = bitNotExpr();
    if (!e) return nullptr;

    Opcode op = makeOperator(Token::BitNot, e.get());
    if (op == Opcode::EXIT) {
      report_.typeError(
          lastLocation(),
          "Type cast error in unary 'not'-operator. Invalid source type <{}>.",
          e->getType());
      return nullptr;
    }

    e = std::make_unique<UnaryExpr>(op, std::move(e), loc.update(end()));
    return e;
  } else {
    return primaryExpr();
  }
}

// primaryExpr ::= literalExpr
//               | variable
//               | function '(' paramList ')'
//               | '(' expr ')'
//               | '[' exprList ']'
std::unique_ptr<Expr> Parser::primaryExpr() {
  switch (token()) {
    case Token::String:
    case Token::RawString:
    case Token::Number:
    case Token::Boolean:
    case Token::IP:
    case Token::Cidr:
    case Token::RegExp:
    case Token::Div: // beginning of regexp
      return literalExpr();
    case Token::RegExpGroup:
      return regexpGroup();
    case Token::StringType:
    case Token::NumberType:
    case Token::BoolType:
      return castExpr();
    case Token::InterpolatedStringFragment:
      return interpolatedStr();
    case Token::Ident: {
      SourceLocation loc = location();
      std::string name = stringValue();
      nextToken();

      std::list<Symbol*> symbols;
      Symbol* symbol = currentScope()->lookup(name, Lookup::All, &symbols);
      if (!symbol) {
        // XXX assume that given symbol is a auto forward-declared handler.
        HandlerSym* href = (HandlerSym*)globalScope()->appendSymbol(
            std::make_unique<HandlerSym>(name, loc));
        return std::make_unique<HandlerRefExpr>(href, loc);
      }

      if (auto variable = dynamic_cast<VariableSym*>(symbol))
        return std::make_unique<VariableExpr>(variable, loc);

      if (auto handler = dynamic_cast<HandlerSym*>(symbol))
        return std::make_unique<HandlerRefExpr>(handler, loc);

      if (symbol->type() == Symbol::BuiltinFunction) {
        std::list<CallableSym*> callables;
        for (Symbol* s : symbols) {
          if (auto c = dynamic_cast<BuiltinFunctionSym*>(s)) {
            callables.push_back(c);
          }
        }

        ParamList params;
        // {{{ parse call params
        if (token() == Token::RndOpen) {
          nextToken();
          if (token() != Token::RndClose) {
            auto ra = paramList();
            if (!ra) {
              return nullptr;
            }
            params = std::move(*ra);
          }
          loc.end = lastLocation().end;
          if (!consume(Token::RndClose)) {
            return nullptr;
          }
        } else if (TokenTraits::isUnaryOp(token()) ||
                   TokenTraits::isLiteral(token()) ||
                   token() == Token::Ident ||
                   token() == Token::BrOpen ||
                   token() == Token::RndOpen) {
          auto ra = paramList();
          if (!ra) {
            return nullptr;
          }
          params = std::move(*ra);
          loc.end = params.location().end;
        }
        // }}}

        return resolve(callables, std::move(params));
      }

      report_.typeError(lastLocation(),
                        "Unsupported symbol type of \"{}\" in expression.",
                        name);
      return nullptr;
    }
    case Token::Begin: {  // lambda-like inline function ref
      static unsigned long i = 0;
      ++i;

      std::string name = fmt::format("__lambda_#{}", i);
      SourceLocation loc = location();
      std::unique_ptr<SymbolTable> st = enterScope(name);
      std::unique_ptr<Stmt> body = compoundStmt();
      leaveScope();

      if (!body)
        return nullptr;

      loc.update(body->location().end);

      HandlerSym* handler = static_cast<HandlerSym*>(currentScope()->appendSymbol(
          std::make_unique<HandlerSym>(name, std::move(st), std::move(body), loc)));

      return std::make_unique<HandlerRefExpr>(handler, loc);
    }
    case Token::RndOpen: {
      SourceLocation loc = location();
      nextToken();
      std::unique_ptr<Expr> e = expr();
      consume(Token::RndClose);
      if (e) {
        e->setLocation(loc.update(end()));
      }
      return e;
    }
    case Token::BrOpen:
      return arrayExpr();
    default:
      report_.syntaxError(lastLocation(), "Unexpected token {}", token());
      return nullptr;
  }
}

std::unique_ptr<Expr> Parser::arrayExpr() {
  SourceLocation loc = location();
  nextToken();  // '['
  std::vector<std::unique_ptr<Expr>> fields;

  if (token() != Token::BrClose) {
    auto e = expr();
    if (!e) return nullptr;

    fields.push_back(std::move(e));

    while (consumeIf(Token::Comma)) {
      e = expr();
      if (!e) return nullptr;

      fields.push_back(std::move(e));
    }
  }

  consume(Token::BrClose);

  if (!fields.empty()) {
    LiteralType baseType = fields.front()->getType();
    for (const auto& e : fields) {
      if (e->getType() != baseType) {
        report_.typeError(lastLocation(),
                          "Mixed element types in array not allowed.");
        return nullptr;
      }
    }

    switch (baseType) {
      case LiteralType::Number:
      case LiteralType::String:
      case LiteralType::IPAddress:
      case LiteralType::Cidr:
        break;
      default:
        report_.typeError(
            lastLocation(),
            "Invalid array expression. Element type {} is not allowed.",
            baseType);
        return nullptr;
    }
  } else {
    report_.typeError(
        lastLocation(),
        "Empty arrays are not allowed. Cannot infer element type.");
    return nullptr;
  }

  return std::make_unique<ArrayExpr>(loc.update(end()), std::move(fields));
}

std::unique_ptr<Expr> Parser::literalExpr() {
  // literalExpr  ::= NUMBER [UNIT]
  //                | BOOL
  //                | STRING
  //                | IP_ADDR
  //                | IP_CIDR
  //                | REGEXP

  static struct {
    const char* ident;
    long long nominator;
    long long denominator;
  } units[] = {{"byte", 1, 1},
               {"kbyte", 1024llu, 1},
               {"mbyte", 1024llu * 1024, 1},
               {"gbyte", 1024llu * 1024 * 1024, 1},
               {"tbyte", 1024llu * 1024 * 1024 * 1024, 1},
               {"bit", 1, 8},
               {"kbit", 1024llu, 8},
               {"mbit", 1024llu * 1024, 8},
               {"gbit", 1024llu * 1024 * 1024, 8},
               {"tbit", 1024llu * 1024 * 1024 * 1024, 8},
               {"sec", 1, 1},
               {"min", 60llu, 1},
               {"hour", 60llu * 60, 1},
               {"day", 60llu * 60 * 24, 1},
               {"week", 60llu * 60 * 24 * 7, 1},
               {"month", 60llu * 60 * 24 * 30, 1},
               {"year", 60llu * 60 * 24 * 365, 1},
               {nullptr, 1, 1}};

  SourceLocation loc(location());

  switch (token()) {
    case Token::Div: {  // /REGEX/
      if (lexer_->continueParseRegEx('/')) {
        auto e = std::make_unique<RegExpExpr>(util::RegExp(stringValue()),
                                              loc.update(end()));
        nextToken();
        return std::move(e);
      } else {
        report_.syntaxError(lastLocation(), "Error parsing regular expression.");
        return nullptr;
      }
    }
    case Token::Number: {  // NUMBER [UNIT]
      auto number = numberValue();
      nextToken();

      if (token() == Token::Ident) {
        std::string sv(stringValue());
        for (size_t i = 0; units[i].ident; ++i) {
          if (sv == units[i].ident ||
              (sv[sv.size() - 1] == 's' &&
               sv.substr(0, sv.size() - 1) == units[i].ident)) {
            nextToken();  // UNIT
            number = number * units[i].nominator / units[i].denominator;
            loc.update(end());
            break;
          }
        }
      }
      return std::make_unique<NumberExpr>(number, loc);
    }
    case Token::Boolean: {
      std::unique_ptr<BoolExpr> e =
          std::make_unique<BoolExpr>(booleanValue(), loc);
      nextToken();
      return std::move(e);
    }
    case Token::String:
    case Token::RawString: {
      std::unique_ptr<StringExpr> e =
          std::make_unique<StringExpr>(stringValue(), loc);
      nextToken();
      return std::move(e);
    }
    case Token::IP: {
      std::unique_ptr<IPAddressExpr> e =
          std::make_unique<IPAddressExpr>(lexer_->ipValue(), loc);
      nextToken();
      return std::move(e);
    }
    case Token::Cidr: {
      std::unique_ptr<CidrExpr> e =
          std::make_unique<CidrExpr>(lexer_->cidr(), loc);
      nextToken();
      return std::move(e);
    }
    case Token::RegExp: {
      std::unique_ptr<RegExpExpr> e =
          std::make_unique<RegExpExpr>(util::RegExp(stringValue()), loc);
      nextToken();
      return std::move(e);
    }
    default:
      report_.typeError(lastLocation(),
                        "Expected literal expression, but got {}.",
                        token());
      return nullptr;
  }
}

std::unique_ptr<Expr> Parser::regexpGroup() {
  FlowNumber groupId = numberValue();
  SourceLocation loc = location();
  consume(Token::RegExpGroup);

  return std::make_unique<RegExpGroupExpr>(loc.update(end()), groupId);
}

std::unique_ptr<ParamList> Parser::paramList() {
  // paramList       ::= namedExpr *(',' namedExpr)
  //                   | expr *(',' expr)

  if (token() == Token::NamedParam) {
    std::unique_ptr<ParamList> args = std::make_unique<ParamList>(true);
    std::string name;
    std::unique_ptr<Expr> e = namedExpr(&name);
    if (!e) return nullptr;

    args->push_back(name, std::move(e));

    while (token() == Token::Comma) {
      nextToken();

      if (token() == Token::RndClose) break;

      e = namedExpr(&name);
      if (!e) return nullptr;

      args->push_back(name, std::move(e));
    }
    return args;
  } else {
    // unnamed param list
    std::unique_ptr<ParamList> args = std::make_unique<ParamList>(false);

    std::unique_ptr<Expr> e = expr();
    if (!e) return nullptr;

    args->push_back(std::move(e));

    while (token() == Token::Comma) {
      nextToken();

      if (token() == Token::RndClose) break;

      e = expr();
      if (!e) return nullptr;

      args->push_back(std::move(e));
    }
    return args;
  }
}

std::unique_ptr<Expr> Parser::namedExpr(std::string* name) {
  // namedExpr       ::= NAMED_PARAM expr

  // FIXME: wtf? what way around?

  *name = stringValue();
  if (!consume(Token::NamedParam)) return nullptr;

  return expr();
}

std::unique_ptr<Expr> asString(std::unique_ptr<Expr>&& expr) {
  LiteralType baseType = expr->getType();
  if (baseType == LiteralType::String) return std::move(expr);

  Opcode opc = makeOperator(Token::StringType, expr.get());
  if (opc == Opcode::EXIT) return nullptr;  // cast error

  return std::make_unique<UnaryExpr>(opc, std::move(expr), expr->location());
}

std::unique_ptr<Expr> Parser::interpolatedStr() {
  SourceLocation sloc(location());
  std::unique_ptr<Expr> result =
      std::make_unique<StringExpr>(stringValue(), sloc.update(end()));
  nextToken();  // interpolation start

  std::unique_ptr<Expr> e(expr());
  if (!e) return nullptr;

  e = asString(std::move(e));
  if (!e) {
    report_.typeError(lastLocation(), "Cast error in string interpolation.");
    return nullptr;
  }

  result = std::make_unique<BinaryExpr>(Opcode::SADD, std::move(result),
                                        std::move(e));

  while (token() == Token::InterpolatedStringFragment) {
    SourceLocation tloc = sloc.update(end());
    result = std::make_unique<BinaryExpr>(
        Opcode::SADD, std::move(result),
        std::make_unique<StringExpr>(stringValue(), tloc));
    nextToken();

    e = expr();
    if (!e) return nullptr;

    e = asString(std::move(e));
    if (!e) {
      report_.typeError(lastLocation(), "Cast error in string interpolation.");
      return nullptr;
    }

    result = std::make_unique<BinaryExpr>(Opcode::SADD, std::move(result),
                                          std::move(e));
  }

  if (!expect(Token::InterpolatedStringEnd)) {
    return nullptr;
  }

  if (!stringValue().empty()) {
    result = std::make_unique<BinaryExpr>(
        Opcode::SADD, std::move(result),
        std::make_unique<StringExpr>(stringValue(), sloc.update(end())));
  }

  nextToken();  // skip InterpolatedStringEnd

  return result;
}

// castExpr ::= 'int' '(' expr ')'
//            | 'string' '(' expr ')'
//            | 'bool' '(' expr ')'
std::unique_ptr<Expr> Parser::castExpr() {
  SourceLocation sloc(location());

  Token targetTypeToken = token();
  nextToken();

  if (!consume(Token::RndOpen)) return nullptr;

  std::unique_ptr<Expr> e(expr());

  if (!consume(Token::RndClose)) return nullptr;

  if (!e) return nullptr;

  Opcode targetType = makeOperator(targetTypeToken, e.get());
  if (targetType == Opcode::EXIT) {
    report_.typeError(
        lastLocation(),
        "Type cast error. No cast implementation found for requested cast from {} to {}.",
        e->getType(), targetTypeToken);
    return nullptr;
  }

  if (targetType == Opcode::NOP) {
    return e;
  }

  // printf("Type cast from %s to %s: %s\n", tos(e->getType()).c_str(),
  // targetTypeToken.c_str(), mnemonic(targetType));
  return std::make_unique<UnaryExpr>(targetType, std::move(e),
                                     sloc.update(end()));
}
// }}}
// {{{ stmt
std::unique_ptr<Stmt> Parser::stmt() {
  switch (token()) {
    case Token::If:
      return ifStmt();
    case Token::While:
      return whileStmt();
    case Token::Match:
      return matchStmt();
    case Token::Begin:
      return compoundStmt();
    case Token::Ident:
      return identStmt();
    case Token::Semicolon: {
      SourceLocation sloc(location());
      nextToken();
      return std::make_unique<CompoundStmt>(sloc.update(end()));
    }
    default:
      report_.syntaxError(lastLocation(),
                          "Unexpected token {}. Expected a statement instead.",
                          token());
      return nullptr;
  }
}

std::unique_ptr<Stmt> Parser::whileStmt() {
  // whileStmt ::= 'while' expr stmt
  SourceLocation sloc(location());

  consume(Token::While);
  std::unique_ptr<Expr> cond = expr();
  std::unique_ptr<Stmt> body = stmt();

  if (!hasFeature(Feature::WhileLoop))
    report_.syntaxError(sloc, "While-loop feature not enabled.");

  return std::make_unique<WhileStmt>(sloc.update(end()), std::move(cond), std::move(body));
}

std::unique_ptr<Stmt> Parser::ifStmt() {
  // ifStmt ::= 'if' expr ['then'] stmt ['else' stmt]
  SourceLocation sloc(location());

  consume(Token::If);
  std::unique_ptr<Expr> cond(expr());
  consumeIf(Token::Then);

  switch (cond->getType()) {
    case LiteralType::Boolean:
      break;
    case LiteralType::String:
      cond = std::make_unique<UnaryExpr>(Opcode::SLEN, std::move(cond), sloc.update(end()));
      cond = std::make_unique<BinaryExpr>(Opcode::NCMPNE, std::move(cond),
          std::make_unique<NumberExpr>(0, sloc));
      break;
    default:
      report_.typeError(
          lastLocation(),
          "If expression must be boolean type. Received type {} instead.",
          cond->getType());
      return nullptr;
  }

  std::unique_ptr<Stmt> thenStmt(stmt());
  if (!thenStmt) return nullptr;

  std::unique_ptr<Stmt> elseStmt;

  if (consumeIf(Token::Else)) {
    elseStmt = stmt();
    if (!elseStmt) {
      return nullptr;
    }
  }

  return std::make_unique<CondStmt>(std::move(cond), std::move(thenStmt),
                                    std::move(elseStmt), sloc.update(end()));
}

std::unique_ptr<Stmt> Parser::matchStmt() {
  // matchStmt       ::= 'match' expr [MATCH_OP] '{' *matchCase ['else' stmt]
  // '}'
  // matchCase       ::= 'on' literalExpr *(',' 'on' literalExpr) stmt
  // MATCH_OP        ::= '==' | '=^' | '=$' | '=~'

  SourceLocation sloc(location());

  if (!consume(Token::Match)) return nullptr;

  auto cond = addExpr();
  if (!cond) return nullptr;

  LiteralType matchType = cond->getType();

  if (matchType != LiteralType::String) {
    report_.typeError(
        lastLocation(),
        "Expected match condition type <{}>, found <{}> instead.",
        LiteralType::String, matchType);
    return nullptr;
  }

  // [MATCH_OP]
  MatchClass op;
  if (TokenTraits::isOperator(token())) {
    switch (token()) {
      case Token::Equal:
        op = MatchClass::Same;
        break;
      case Token::PrefixMatch:
        op = MatchClass::Head;
        break;
      case Token::SuffixMatch:
        op = MatchClass::Tail;
        break;
      case Token::RegexMatch:
        op = MatchClass::RegExp;
        break;
      default:
        report_.typeError(
            lastLocation(),
            "Expected match operator, found token <{}> instead.",
            token());
        return nullptr;
    }
    nextToken();
  } else {
    op = MatchClass::Same;
  }

  if (op == MatchClass::RegExp) matchType = LiteralType::RegExp;

  // '{'
  if (!consume(Token::Begin)) return nullptr;

  // *('on' literalExpr *(',' 'on' literalExpr) stmt)
  MatchStmt::CaseList cases;
  do {
    if (!consume(Token::On)) {
      return nullptr;
    }

    MatchStmt::Case one;

    // first label
    auto label = literalExpr();
    if (!label) return nullptr;

    one.first.push_back(std::move(label));

    // any more labels
    while (consumeIf(Token::Comma)) {
      if (!consume(Token::On)) return nullptr;

      label = literalExpr();
      if (!label) return nullptr;

      one.first.push_back(std::move(label));
    }

    for (auto& label : one.first) {
      LiteralType caseType = label->getType();
      if (matchType != caseType) {
        report_.typeError(
            lastLocation(),
            "Type mismatch in match-on statement. Expected <{}> but got <{}>.",
            matchType, caseType);
        return nullptr;
      }
    }

    one.second = stmt();
    if (!one.second) return nullptr;

    cases.push_back(std::move(one));
  } while (token() == Token::On);

  // ['else' stmt]
  std::unique_ptr<Stmt> elseStmt;
  if (consumeIf(Token::Else)) {
    elseStmt = stmt();
    if (!elseStmt) {
      return nullptr;
    }
  }

  // '}'
  if (!consume(Token::End)) return nullptr;

  return std::make_unique<MatchStmt>(sloc.update(end()), std::move(cond), op,
                                     std::move(cases), std::move(elseStmt));
}

// compoundStmt ::= '{' varDecl* stmt* '}'
std::unique_ptr<Stmt> Parser::compoundStmt() {
  SourceLocation sloc(location());
  nextToken();  // '{'

  std::unique_ptr<CompoundStmt> cs = std::make_unique<CompoundStmt>(sloc);

  while (token() == Token::Var) {
    if (std::unique_ptr<VariableSym> var = varDecl())
      currentScope()->appendSymbol(std::move(var));
    else
      return nullptr;
  }

  for (;;) {
    if (consumeIf(Token::End)) {
      cs->location().update(end());
      return std::unique_ptr<Stmt>(cs.release());
    }

    if (std::unique_ptr<Stmt> s = stmt())
      cs->push_back(std::move(s));
    else
      return nullptr;
  }
}

std::unique_ptr<Stmt> Parser::identStmt() {
  // identStmt  ::= callStmt | assignStmt
  // callStmt   ::= NAME ['(' paramList ')' | paramList] (';' | LF)
  // assignStmt ::= NAME '=' expr [';' | LF]
  //
  // NAME may be a builtin-function, builtin-handler, handler-name, or variable.

  SourceLocation loc(location());
  std::string name = stringValue();
  nextToken();  // IDENT

  std::unique_ptr<Stmt> stmt;
  std::list<Symbol*> symbols;
  Symbol* callee = currentScope()->lookup(name, Lookup::All, &symbols);
  if (!callee) {
    // XXX assume that given symbol is a auto forward-declared handler that's
    // being defined later in the source.
    if (token() != Token::Semicolon) {
      report_.typeError(lastLocation(), "Unknown symbol '{}'.", name);
      return nullptr;
    }

    callee = (HandlerSym*)globalScope()->appendSymbol(
        std::make_unique<HandlerSym>(name, loc));
    symbols.push_back(callee);
  }

  switch (callee->type()) {
    case Symbol::Variable: {  // var '=' expr (';' | LF)
      if (!consume(Token::Assign)) return nullptr;

      std::unique_ptr<Expr> value = expr();
      if (!value) return nullptr;

      VariableSym* var = static_cast<VariableSym*>(callee);
      LiteralType leftType = var->initializer()->getType();
      LiteralType rightType = value->getType();
      if (leftType != rightType) {
        report_.typeError(
            lastLocation(),
            "Type mismatch in assignment. Expected <{}> but got <{}>.",
            leftType, rightType);
        return nullptr;
      }

      stmt = std::make_unique<AssignStmt>(var, std::move(value),
                                          loc.update(end()));
      break;
    }
    case Symbol::BuiltinFunction:
    case Symbol::BuiltinHandler: {
      auto call = callStmt(symbols);
      if (!call) {
        return nullptr;
      }
      stmt = std::make_unique<ExprStmt>(std::move(call));
      break;
    }
    case Symbol::Handler:
      stmt = std::make_unique<ExprStmt>(
          std::make_unique<CallExpr>(loc, (CallableSym*)callee, ParamList()));
      break;
    default:
      break;
  }

  // postscript statement handling
  if (testTokens(Token::If, Token::Unless))
      return postscriptStmt(std::move(stmt));

  if (!consume(Token::Semicolon))
    return nullptr;

  return stmt;
}

std::unique_ptr<CallExpr> Parser::callStmt(
    const std::list<Symbol*>& symbols) {
  // callStmt ::= NAME ['(' paramList ')' | paramList] (';' | LF)
  // namedArg ::= NAME ':' expr

  std::list<CallableSym*> callables;
  for (Symbol* s : symbols) {
    if (auto c = dynamic_cast<CallableSym*>(s)) {
      callables.push_back(c);
    }
  }

  if (callables.empty()) {
    report_.typeError(lastLocation(), "Symbol is not callable.");  // XXX should never reach here
    return nullptr;
  }

  ParamList params;
  SourceLocation loc = location();

  // {{{ parse call params
  if (token() == Token::RndOpen) {
    nextToken();
    if (token() != Token::RndClose) {
      auto ra = paramList();
      if (!ra) {
        return nullptr;
      }
      params = std::move(*ra);
    }
    loc.end = lastLocation().end;
    if (!consume(Token::RndClose)) {
      return nullptr;
    }
  } else if (token() != Token::Semicolon && token() != Token::If &&
             token() != Token::Unless) {
    auto ra = paramList();
    if (!ra) {
      return nullptr;
    }
    params = std::move(*ra);
    loc.end = params.location().end;
  }
  // }}}

  return resolve(callables, std::move(params));
}

Signature makeSignature(const CallableSym* callee, const ParamList& params) {
  Signature sig;

  sig.setName(callee->name());

  std::vector<LiteralType> argTypes;
  for (const std::unique_ptr<Expr>& arg : params.values()) {
    argTypes.push_back(arg->getType());
  }

  sig.setArgs(argTypes);

  return sig;
};

std::unique_ptr<CallExpr> Parser::resolve(
    const std::list<CallableSym*>& callables, ParamList&& params) {
  Signature inputSignature = makeSignature(callables.front(), params);

  // attempt to find a full match first
  for (CallableSym* callee : callables) {
    if (callee->isDirectMatch(params)) {
      return std::make_unique<CallExpr>(callee->location(), callee,
                                        std::move(params));
    }
  }

  // attempt to find something with default values or parameter-reordering (if
  // named args)
  std::list<CallableSym*> result;
  std::list<std::pair<CallableSym*, std::string>> matchErrors;

  for (CallableSym* callee : callables) {
    std::string msg;
    if (callee->tryMatch(params, &msg)) {
      result.push_back(callee);
    } else {
      matchErrors.emplace_back(callee, msg);
    }
  }

  if (result.empty()) {
    report_.typeError(lastLocation(), "No matching signature for {}.", inputSignature);
    for (const auto& me : matchErrors) {
      report_.typeError(lastLocation(), me.second);
    }
    return nullptr;
  }

  if (result.size() > 1) {
    report_.typeError(lastLocation(), "Call to builtin is ambiguous.");
    return nullptr;
  }

  CallableSym* callableSym = result.front();

  if (callableSym->nativeCallback()->isExperimental()) {
    report_.warning(lastLocation(),
                    "Using experimental builtin API {}.",
                    callableSym->nativeCallback()->signature());
  }

  return std::make_unique<CallExpr>(callableSym->location(), callableSym,
                                    std::move(params));
}

std::unique_ptr<Stmt> Parser::postscriptStmt(
    std::unique_ptr<Stmt> baseStmt) {
  Token op = token();
  switch (op) {
    case Token::If:
    case Token::Unless:
      break;
    default:
      return baseStmt;
  }

  // STMT ['if' EXPR] ';'
  // STMT ['unless' EXPR] ';'

  SourceLocation sloc = location();

  nextToken();  // 'if' | 'unless'

  std::unique_ptr<Expr> condExpr = expr();
  if (!condExpr) return nullptr;

  if (op == Token::Unless) {
    auto opc = makeOperator(Token::Not, condExpr.get());
    if (opc == Opcode::EXIT) {
      report_.typeError(
          lastLocation(),
          "Type cast error. No cast implementation found for requested cast "
          "from {} to {}.",
          condExpr->getType(), LiteralType::Boolean);
      return nullptr;
    }

    condExpr = std::make_unique<UnaryExpr>(opc, std::move(condExpr), sloc);
  }

  if (!consume(Token::Semicolon)) return nullptr;

  return std::make_unique<CondStmt>(std::move(condExpr), std::move(baseStmt),
                                    nullptr, sloc.update(end()));
}
// }}}

}  // namespace flow
