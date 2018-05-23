// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/lang/AST.h>
#include <flow/lang/CallVisitor.h>

#include <algorithm>
#include <cassert>

namespace flow::lang {

CallVisitor::CallVisitor(ASTNode* root) : calls_() { visit(root); }

CallVisitor::~CallVisitor() {}

void CallVisitor::visit(ASTNode* node) {
  if (node) {
    node->visit(*this);
  }
}

// {{{ symbols
void CallVisitor::accept(VariableSym& variable) {
  visit(variable.initializer());
}

void CallVisitor::accept(HandlerSym& handler) {
  if (handler.scope()) {
    for (std::unique_ptr<Symbol>& sym : *handler.scope()) {
      visit(sym.get());
    }
  }

  visit(handler.body());
}

void CallVisitor::accept(BuiltinFunctionSym& v) {}

void CallVisitor::accept(BuiltinHandlerSym& v) {}

void CallVisitor::accept(UnitSym& unit) {
  for (std::unique_ptr<Symbol>& s : *unit.scope()) {
    visit(s.get());
  }
}
// }}}
// {{{ expressions
void CallVisitor::accept(UnaryExpr& expr) { visit(expr.subExpr()); }

void CallVisitor::accept(BinaryExpr& expr) {
  visit(expr.leftExpr());
  visit(expr.rightExpr());
}

void CallVisitor::accept(CallExpr& call) {
  for (const auto& arg : call.args().values()) {
    visit(arg.get());
  }

  if (call.callee() && call.callee()->isBuiltin()) {
    calls_.push_back(&call);
  }
}

void CallVisitor::accept(VariableExpr& expr) {}

void CallVisitor::accept(HandlerRefExpr& expr) {}

void CallVisitor::accept(StringExpr& expr) {}

void CallVisitor::accept(NumberExpr& expr) {}

void CallVisitor::accept(BoolExpr& expr) {}

void CallVisitor::accept(RegExpExpr& expr) {}

void CallVisitor::accept(IPAddressExpr& expr) {}

void CallVisitor::accept(CidrExpr& expr) {}

void CallVisitor::accept(ArrayExpr& array) {
  for (const auto& e : array.values()) {
    visit(e.get());
  }
}

void CallVisitor::accept(ExprStmt& stmt) { visit(stmt.expression()); }
// }}}
// {{{ stmt
void CallVisitor::accept(CompoundStmt& compound) {
  for (const auto& stmt : compound) {
    visit(stmt.get());
  }
}

void CallVisitor::accept(CondStmt& condStmt) {
  visit(condStmt.condition());
  visit(condStmt.thenStmt());
  visit(condStmt.elseStmt());
}

void CallVisitor::accept(WhileStmt& whileStmt) {
  visit(whileStmt.condition());
  visit(whileStmt.bodyStmt());
}

void CallVisitor::accept(MatchStmt& stmt) {
  visit(stmt.condition());
  for (auto& one : stmt.cases()) {
    for (auto& label : one.first) visit(label.get());

    visit(one.second.get());
  }
  visit(stmt.elseStmt());
}

void CallVisitor::accept(AssignStmt& assignStmt) {
  visit(assignStmt.expression());
}
// }}}

}  // namespace flow
