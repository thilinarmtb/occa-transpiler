#include "oklt/core/ast_traversal/ast_visitor.h"
#include "oklt/core/transpile_session/transpile_session.h"
#include "oklt/core/attribute_manager/attribute_manager.h"

namespace oklt {
using namespace clang;

ASTVisitor::ASTVisitor(TranspileSession &session)
    :_session(session)
{}

bool ASTVisitor::TraverseDecl(Decl *decl) {
  return RecursiveASTVisitor<ASTVisitor>::TraverseDecl(decl);
}

bool ASTVisitor::TraverseStmt(Stmt *stmt, DataRecursionQueue *queue) {
  if(!stmt) {
    return true;
  }
  if (stmt->getStmtClass() != Stmt::AttributedStmtClass) {
    return RecursiveASTVisitor<ASTVisitor>::TraverseStmt(stmt);
  }
  AttributedStmt *attrStmt = cast<AttributedStmt>(stmt);
  const Stmt *subStmt = attrStmt->getSubStmt();
  auto &attrManager = _session.getAttrManager();
  llvm::Expected<const Attr*> expectedAttr = attrManager.checkAttrs(attrStmt->getAttrs(),
                                                               stmt,
                                                               _session);
  if(!expectedAttr) {
    auto &errorReporter = _session.getErrorReporter();
    auto errorDescription = toString(expectedAttr.takeError());
    errorReporter.emitError(stmt->getSourceRange(),errorDescription);
    return false;
  }
  const Attr* attr = expectedAttr.get();
  //INFO: no OKL attributes to process, continue
  if(!attr) {
    return RecursiveASTVisitor<ASTVisitor>::TraverseStmt(stmt);
  }
  return attrManager.handleAttr(attr, subStmt, _session);
}

bool ASTVisitor::VisitFunctionDecl(FunctionDecl *funcDecl) {
  auto &attrManager = _session.getAttrManager();
  llvm::Expected<const Attr*> expectedAttr = attrManager.checkAttrs(funcDecl->getAttrs(),
                                                                    funcDecl,
                                                                    _session);
  if(!expectedAttr) {
    auto &errorReporter = _session.getErrorReporter();
    auto errorDescription = toString(expectedAttr.takeError());
    errorReporter.emitError(funcDecl->getSourceRange(),errorDescription);
    return false;
  }
  const Attr* attr = expectedAttr.get();
  //INFO: no OKL attributes to process, continue
  if(!attr) {
    return true;
  }
  return attrManager.handleAttr(attr, funcDecl, _session);
}

bool ASTVisitor::VisitVarDecl(VarDecl *varDecl) {
  auto &attrManager = _session.getAttrManager();
  llvm::Expected<const Attr*> expectedAttr = attrManager.checkAttrs(varDecl->getAttrs(),
                                                                     varDecl,
                                                                     _session);
  if(!expectedAttr) {
    auto &errorReporter = _session.getErrorReporter();
    auto errorDescription = toString(expectedAttr.takeError());
    errorReporter.emitError(varDecl->getSourceRange(),errorDescription);
    return false;
  }
  const Attr* attr = expectedAttr.get();
  //INFO: no OKL attributes to process, continue
  if(!attr) {
    return true;
  }
  return attrManager.handleAttr(attr, varDecl, _session);
}

}
