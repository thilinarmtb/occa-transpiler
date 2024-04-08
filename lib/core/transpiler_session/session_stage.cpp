#include "core/transpiler_session/session_stage.h"
#include "core/attribute_manager/attribute_manager.h"
#include "core/diag/diag_consumer.h"
#include "core/transpiler_session/transpiler_session.h"

#include <spdlog/spdlog.h>

#include <clang/AST/ParentMapContext.h>
#include <clang/Basic/SourceManager.h>

namespace oklt {
using namespace clang;

SessionStage::SessionStage(TranspilerSession& session,
                           CompilerInstance& compiler,
                           RewriterProxyType rwType)
    : _session(session),
      _compiler(compiler),
      _backend(session.input.backend),
      _astProcType(session.input.astProcType),
      _rewriter(makeRewriterProxy(_compiler.getSourceManager(), _compiler.getLangOpts(), rwType)) {}

clang::CompilerInstance& SessionStage::getCompiler() {
    return _compiler;
}

oklt::Rewriter& SessionStage::getRewriter() {
    return *_rewriter.get();
}

AttributeManager& SessionStage::getAttrManager() {
    return AttributeManager::instance();
}

void SessionStage::setLauncherMode() {
    _rewriter =
        std::make_unique<oklt::Rewriter>(_compiler.getSourceManager(), _compiler.getLangOpts());
    _backend = TargetBackend::_LAUNCHER;
}

std::string SessionStage::getRewriterResultForMainFile() {
    const auto& sm = _compiler.getSourceManager();
    auto mainFID = sm.getMainFileID();
    auto* rewriteBuf = _rewriter->getRewriteBufferFor(mainFID);
    if (!rewriteBuf || rewriteBuf->size() == 0) {
        return sm.getBufferData(mainFID).str();
    }

    std::string mainContent{rewriteBuf->begin(), rewriteBuf->end()};

    return mainContent;
}

TransformedFiles SessionStage::getRewriterResultForHeaders() {
    TransformedFiles headers;
    const auto& sm = _compiler.getSourceManager();
    auto mainFID = sm.getMainFileID();

    // rewriter cache each include of the same header as separate buffer
    // ensure that only first hopefully modified is taken
    std::set<std::string> processedFID;
    for (auto it = _rewriter->buffer_begin(); it != _rewriter->buffer_end(); ++it) {
        const auto& [fid, buf] = *it;

        // sanity check
        if (fid.isInvalid()) {
            continue;
        }

        // skip main source file
        if (fid == mainFID) {
            continue;
        }

        auto* fileEntry = sm.getFileEntryForID(fid);
        if (!fileEntry) {
            continue;
        }
        auto fileName = fileEntry->getName().str();
        if (processedFID.count(fileName)) {
            continue;
        }

        headers.fileMap[fileName] = [](const auto& buf) -> std::string {
            if (buf.size() == 0) {
                return "";
            }
            return std::string{buf.begin(), buf.end()};
        }(buf);
        processedFID.insert(fileName);
    }

    return headers;
}

TargetBackend SessionStage::getBackend() const {
    return _backend;
}

AstProcessorType SessionStage::getAstProccesorType() const {
    return _astProcType;
}

void SessionStage::pushDiagnosticMessage(clang::StoredDiagnostic& message) {
    _session.pushDiagnosticMessage(message, *this);
}

void SessionStage::pushError(std::error_code ec, std::string desc) {
    _session.pushError(ec, std::move(desc));
}

void SessionStage::pushError(const Error& err) {
    _session.pushError(err.ec, std::move(err.desc));
}

void SessionStage::pushError(const Error& err, const SourceRange& arange) {
    // Diagnostic diag(&getCompiler().getDiagnostics(), err.desc);
    auto& SM = getCompiler().getSourceManager();
    auto begLoc = arange.getBegin();

    StoredDiagnostic sd(
        DiagnosticsEngine::Level::Error, 0, err.desc, FullSourceLoc(begLoc, SM), {}, {});
    _session.pushDiagnosticMessage(sd, *this);
}

void SessionStage::pushWarning(std::string desc) {
    _session.pushWarning(std::move(desc));
}

SessionStage* getStageFromASTContext(clang::ASTContext& ast) {
    // NOTE:
    // There are a few stable references/pointer that can point to our controlled classes and
    // structures. getSourceManager().getFileManager -- Reference to FileManager. Initialized before
    // CompilerInstance, exist only one. getDiagnostics().getClient() -- Pointer to
    // DiagnosticConsumer. Initialized during ExecuteAction, can be multiplexed.

    auto diag = dynamic_cast<DiagConsumer*>(ast.getDiagnostics().getClient());
    if (!diag) {
        return nullptr;
    }

    return &diag->getSession();
}

}  // namespace oklt
