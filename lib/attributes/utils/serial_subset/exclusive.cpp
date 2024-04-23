#include "attributes/utils/common.h"
#include "attributes/utils/default_handlers.h"
#include "core/attribute_manager/attribute_manager.h"
#include "core/sema/okl_sema_ctx.h"
#include "core/transpiler_session/session_stage.h"
#include "core/utils/attributes.h"

#include <spdlog/spdlog.h>

namespace oklt::serial_subset {
using namespace clang;

namespace {
const std::string outerLoopText = "\nint _occa_exclusive_index;";
const std::string exlusiveExprText = "[_occa_exclusive_index]";
}  // namespace

HandleResult handleExclusiveDeclAttribute(const Attr& a, const VarDecl& decl, SessionStage& s) {
    SPDLOG_DEBUG("Handle [@exclusive] attribute (decl)");

    auto& sema = s.tryEmplaceUserCtx<OklSemaCtx>();
    auto loopInfo = sema.getLoopInfo();
    if (!loopInfo) {
        return tl::make_unexpected(
            Error{{}, "@exclusive: failed to fetch loop meta data from sema"});
    }

    auto compStmt = dyn_cast_or_null<CompoundStmt>(loopInfo->stmt.getBody());
    auto definedBetweenOuterInner = isLastOuter(loopInfo);
    if (!compStmt || !definedBetweenOuterInner) {
        return tl::make_unexpected(
            Error{{}, "Must define [@exclusive] variables between [@outer] and [@inner] loops"});
    }

    auto& rewriter = s.getRewriter();

    SourceRange attrRange = getAttrFullSourceRange(a);
    rewriter.RemoveText(attrRange);

    if (!loopInfo->exclusiveInfo.declared) {
        auto indexLoc = compStmt->getLBracLoc().getLocWithOffset(1);
        rewriter.InsertTextAfter(indexLoc, outerLoopText);
    }

    // Find max size of inner loops
    size_t sz = 0;
    for (auto child : loopInfo->children) {
        auto v = child.getInnerSizes();
        if (v.hasNullOpts()) {
            sz = 1024;
            break;
        }
        sz = std::max(v.product(), sz);
    }
    std::string varSuffix = "[" + std::to_string(sz) + "]";

    // Add size and wrap initialization.
    auto nameLoc = decl.getLocation().getLocWithOffset(decl.getName().size());
    rewriter.InsertTextAfter(nameLoc, varSuffix);
    if (decl.hasInit()) {
        auto expr = decl.getInit();
        rewriter.InsertTextBefore(expr->getBeginLoc(), "{");
        rewriter.InsertTextAfter(decl.getEndLoc().getLocWithOffset(1), "}");
    }

    return defaultHandleExclusiveDeclAttribute(a, decl, s);
}

HandleResult handleExclusiveExprAttribute(const Attr& a, const DeclRefExpr& expr, SessionStage& s) {
    SPDLOG_DEBUG("Handle [@exclusive] attribute (stmt)");

    auto& sema = s.tryEmplaceUserCtx<OklSemaCtx>();
    auto loopInfo = sema.getLoopInfo();
    if (!loopInfo) {
        return tl::make_unexpected(
            Error{{}, "@exclusive: failed to fetch loop meta data from sema"});
    }

    auto loc = expr.getLocation().getLocWithOffset(expr.getNameInfo().getAsString().size());
    s.getRewriter().InsertTextAfter(loc, exlusiveExprText);
    return defaultHandleExclusiveStmtAttribute(a, expr, s);
}

}  // namespace oklt::serial_subset
