// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/Parser.h>
#include <flow/ASTPrinter.h>
#include <xzero/testing.h>
#include <memory>

using namespace xzero;
using namespace flow;

TEST(Parser, handlerDecl) {
  auto parser = std::make_shared<Parser>(nullptr, nullptr, nullptr);
  parser->openString("handler main {}");
  std::unique_ptr<UnitSym> unit = parser->parse();

  auto h = unit->findHandler("main");
  ASSERT_TRUE(h != nullptr);

  CompoundStmt* body = dynamic_cast<CompoundStmt*>(h->body());
  ASSERT_TRUE(body != nullptr);
  EXPECT_TRUE(body->empty());
}

TEST(Parser, varDecl) {
  auto parser = std::make_shared<Parser>(nullptr, nullptr, nullptr);
  parser->openString("handler main { var i = 42; }");
  std::unique_ptr<UnitSym> unit = parser->parse();

  auto h = unit->findHandler("main");
  ASSERT_TRUE(h != nullptr);

  ASSERT_NE(nullptr, h->scope()->lookup("i", Lookup::Self));
  Symbol* var = h->scope()->lookup("i", Lookup::Self);

  ASSERT_TRUE(var != nullptr);
  EXPECT_EQ("i", var->name());
}

// TEST(Parser, logicExpr) {} // TODO
// TEST(Parser, notExpr) {} // TODO
// TEST(Parser, relExpr) {} // TODO
// TEST(Parser, addExpr) {} // TODO
// TEST(Parser, bitNotExpr) {} // TODO
// TEST(Parser, primaryExpr) {} // TODO
// TEST(Parser, arrayExpr) {} // TODO
// TEST(Parser, literalExpr) {} // TODO
// TEST(Parser, interpolatedStringExpr) {} // TODO
// TEST(Parser, castExpr) {} // TODO
// TEST(Parser, ifStmt) {} // TODO
// TEST(Parser, matchStmt) {} // TODO
// TEST(Parser, compoundStmt) {} // TODO
// TEST(Parser, callStmt) {} // TODO
// TEST(Parser, postscriptStmt) {} // TODO
