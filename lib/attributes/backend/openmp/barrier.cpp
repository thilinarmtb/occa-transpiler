#include "attributes/backend/openmp/common.h"

namespace {
using namespace oklt;
using namespace clang;

HandleResult handleOPENMPBarrierAttribute(const Attr& a, const NullStmt& stmt, SessionStage& s) {
#ifdef TRANSPILER_DEBUG_LOG
    llvm::outs() << "handle attribute: " << a.getNormalizedFullName() << '\n';
#endif

    SourceRange range(getAttrFullSourceRange(a).getBegin(), stmt.getEndLoc());
    s.getRewriter().RemoveText(range);
    return {};
}

__attribute__((constructor)) void registerOPENMPBarrierHandler() {
    auto ok = oklt::AttributeManager::instance().registerBackendHandler(
        {TargetBackend::OPENMP, BARRIER_ATTR_NAME},
        makeSpecificAttrHandle(handleOPENMPBarrierAttribute));

    if (!ok) {
        llvm::errs() << "failed to register " << BARRIER_ATTR_NAME
                     << " attribute handler (OpenMP)\n";
    }
}
}  // namespace