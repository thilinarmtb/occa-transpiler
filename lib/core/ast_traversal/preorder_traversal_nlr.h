#pragma once

#include "core/transpilation.h"

#include <clang/AST/RecursiveASTVisitor.h>
#include <tl/expected.hpp>

namespace oklt {

class SessionStage;
class AstProcessorManager;
class TranspilationCtx;

class PreorderNlrTraversal : public clang::RecursiveASTVisitor<PreorderNlrTraversal> {
   public:
    explicit PreorderNlrTraversal(AstProcessorManager& procMng, SessionStage& stage);
    bool TraverseDecl(clang::Decl* decl);
    bool TraverseStmt(clang::Stmt* stmt);
    bool TraverseRecoveryExpr(clang::RecoveryExpr* recoveryExpr);
    bool TraverseTranslationUnitDecl(clang::TranslationUnitDecl* translationUnitDecl);

    tl::expected<std::string, std::error_code> applyAstProccessor(clang::TranslationUnitDecl*);

   private:
    AstProcessorManager& _procMng;
    SessionStage& _stage;
    Transpilations _trasnpilations;
};

}  // namespace oklt
