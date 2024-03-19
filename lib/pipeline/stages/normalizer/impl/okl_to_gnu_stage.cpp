#include <oklt/core/error.h>

#include "core/transpiler_session/session_stage.h"
#include "core/vfs/overlay_fs.h"

#include "pipeline/stages/normalizer/error_codes.h"
#include "pipeline/stages/normalizer/impl/okl_attr_traverser.h"
#include "pipeline/stages/normalizer/impl/okl_to_gnu_stage.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Analysis/MacroExpansionContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/Tooling.h>

#include <set>

namespace {

using namespace clang;
using namespace oklt;

std::set<std::string> gnuExcepStmtAttrs{"atomic", "shared"};

bool isProbablyOklSpecificForStmt(Token left, Token right) {
    return left.is(tok::semi) && right.is(tok::r_paren);
}

bool isProbablyAtBeginnigOfExpr(Token left, Token right) {
    return ((left.is(tok::semi) || left.is(tok::l_brace)) && !right.is(tok::semi));
}

Token getLeftNeigbour(const OklAttribute& attr, const std::vector<Token>& tokens) {
    return attr.tok_indecies.front() != 0 ? tokens[attr.tok_indecies.front() - 1] : Token();
}

Token getRightNeigbour(const OklAttribute& attr, const std::vector<Token>& tokens) {
    return attr.tok_indecies.back() != tokens.size() ? tokens[attr.tok_indecies.back() + 1]
                                                     : Token();
}

void removeOklAttr(const std::vector<Token>& tokens, const OklAttribute& attr, Rewriter& rewriter) {
    // remove OKL specific attribute in source code
    SourceLocation attrLocStart(tokens[attr.tok_indecies.front()].getLocation());
    SourceLocation attrLocEnd(tokens[attr.tok_indecies.back()].getLastLoc());
    SourceRange attrSrcRange(attrLocStart, attrLocEnd);
    rewriter.RemoveText(attrSrcRange);
}

OklAttrMarker makeOklAttrMarker(const Preprocessor& pp,
                                const OklAttribute& oklAtr,
                                const SourceLocation loc) {
    return {.attr = oklAtr,
            .loc = {.line = pp.getSourceManager().getPresumedLineNumber(loc),
                    .col = pp.getSourceManager().getPresumedColumnNumber(loc)}};
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// routine to replace OKL attribute with GNU one and store it original source location
// one trick is that functions could fix malformed C++ for statement with extra semi
bool replaceOklByGnuAttribute(std::list<OklAttrMarker>& gnu_markers,
                              std::list<OklAttrMarker>& recovery_markers,
                              const OklAttribute& oklAttr,
                              const std::vector<Token>& tokens,
                              Preprocessor& pp,
                              Rewriter& rewriter) {
    // TODO log each mofidification to adjust marker line col coordinate accordinagly
    removeOklAttr(tokens, oklAttr, rewriter);

    auto leftNeigbour = getLeftNeigbour(oklAttr, tokens);
    auto rightNeighbour = getRightNeigbour(oklAttr, tokens);
    auto insertLoc(tokens[oklAttr.tok_indecies.front()].getLocation());

    // fix malformed C++ syntax like for(init;cond;step;@outer) to for(init;cond;step) and mark
    // source location to fix it during AST traversal
    if (isProbablyOklSpecificForStmt(leftNeigbour, rightNeighbour)) {
        rewriter.ReplaceText(leftNeigbour.getLocation(), 1, ")");
        rewriter.ReplaceText(rightNeighbour.getLocation(), 1, " ");
        recovery_markers.emplace_back(makeOklAttrMarker(pp, oklAttr, leftNeigbour.getLocation()));
    }
    // INFO: just replace directly with standard attribute if it's originally at the beginnig
    else if (isProbablyAtBeginnigOfExpr(leftNeigbour, rightNeighbour)) {
        auto cppAttr = wrapAsSpecificCxxAttr(oklAttr);
        rewriter.InsertTextBefore(insertLoc, cppAttr);
    }
    // INFO: attribute is not at the beginning of expr so wrap it as GNU.
    // GNU attribute has more diverisity for locations(and it's nigtmare for parser and AST to
    // handle all cases). than standard attribute. After parsing AST GNU will be replaced by CXX NB:
    // there are cases where GNU attribute could not be parsed and embed into AST For example:
    //   a+=1 __attrbute((okl_atomic));
    // and
    //   expected unqualified-id
    // error will be generated by clang AST parser
    else {
        auto gnuAttr = wrapAsSpecificGnuAttr(oklAttr);
        rewriter.InsertTextBefore(insertLoc, gnuAttr);
        gnu_markers.emplace_back(makeOklAttrMarker(pp, oklAttr, insertLoc));
    }

#ifdef NORMALIZER_DEBUG_LOG
    llvm::outs() << "removed attr: " << oklAttr.name
                 << " at loc: " << insertLoc.printToString(pp.getSourceManager()) << '\n';
#endif

    return true;
}

std::vector<Token> fetchTokens(Preprocessor& pp) {
    std::vector<Token> tokens;
    while (true) {
        Token tok{};
        pp.Lex(tok);

        if (tok.is(tok::eof)) {
            break;
        }

        if (tok.is(tok::unknown)) {
            // Check for '@' symbol
            auto spelling = pp.getSpelling(tok);
            if (spelling.empty() || spelling[0] != OKL_ATTR_MARKER) {
                break;
            }
            tok.setKind(tok::at);
        }

        tokens.push_back(tok);
    }

    return tokens;
}

struct OklToGnuAttributeNormalizerAction : public clang::ASTFrontendAction {
    explicit OklToGnuAttributeNormalizerAction(OklToGnuStageInput& input,
                                               OklToGnuStageOutput& output)
        : _input(input),
          _output(output),
          _session(*input.session) {
        (void)_input;
    }

   protected:
    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& compiler,
                                                          llvm::StringRef in_file) override {
        return nullptr;
    }

    bool BeginSourceFileAction(CompilerInstance& compiler) override {
        if (compiler.hasFileManager()) {
            auto overlayFs = makeOverlayFs(compiler.getFileManager().getVirtualFileSystemPtr(),
                                           _input.oklCppIncs);
            compiler.getFileManager().setVirtualFileSystem(overlayFs);
        }

        auto& pp = compiler.getPreprocessor();
        pp.EnterMainSourceFile();
        auto tokens = fetchTokens(pp);

        if (tokens.empty()) {
            _session.pushError(OkltNormalizerErrorCode::EMPTY_SOURCE_STRING,
                               "no tokens in source?");
            return false;
        }

        SessionStage stage{_session, compiler};
        auto& rewriter = stage.getRewriter();

        auto result = visitOklAttributes(
            tokens,
            pp,
            [this, &rewriter](
                const OklAttribute& attr, const std::vector<Token>& tokens, Preprocessor& pp) {
                replaceOklByGnuAttribute(
                    _output.gnuMarkers, _output.recoveryMarkers, attr, tokens, pp, rewriter);
                return true;
            });
        if (!result) {
            _session.pushError(result.error().ec, result.error().desc);
            return false;
        }

        _output.gnuCppSrc = stage.getRewriterResultForMainFile();
        // no errors and empty output could mean that the source is already normalized
        // so use input as output and lets the next stage try to figure out
        if (_output.gnuCppSrc.empty()) {
            _output.gnuCppSrc = std::move(_input.oklCppSrc);
        }

        // we need keep all headers in output even there are not modififcation by rewriter to
        // populate affected files futher
        _output.gnuCppIncs = stage.getRewriterResultForHeaders();
        _output.gnuCppIncs.fileMap.merge(_input.oklCppIncs.fileMap);

        pp.EndSourceFile();

        return false;
    }

   private:
    OklToGnuStageInput& _input;
    OklToGnuStageOutput& _output;
    TranspilerSession& _session;
};
}  // namespace
namespace oklt {

OklToGnuResult convertOklToGnuAttribute(OklToGnuStageInput input) {
    if (input.oklCppSrc.empty()) {
        llvm::outs() << "input source string is empty\n";
        auto error =
            makeError(OkltNormalizerErrorCode::EMPTY_SOURCE_STRING, "input source string is empty");
        return tl::make_unexpected(std::vector<Error>{error});
    }

#ifdef NORMALIZER_DEBUG_LOG
    llvm::outs() << "stage 0 OKL source:\n\n" << input.oklCppSrc << '\n';
#endif

    Twine tool_name = "okl-transpiler-normalization-to-gnu";
    Twine file_name("main_kernel.cpp");
    std::vector<std::string> args = {"-std=c++17", "-fparse-all-comments", "-I."};

    auto input_file = std::move(input.oklCppSrc);

    auto& sessionInput = input.session->input;
    for (const auto& define : sessionInput.defines) {
        std::string def = "-D" + define;
        args.push_back(std::move(def));
    }

    for (const auto& includePath : sessionInput.inlcudeDirectories) {
        std::string incPath = "-I" + includePath.string();
        args.push_back(std::move(incPath));
    }

    OklToGnuStageOutput output = {.session = input.session};
    auto ok = tooling::runToolOnCodeWithArgs(
        std::make_unique<OklToGnuAttributeNormalizerAction>(input, output),
        input_file,
        args,
        file_name,
        tool_name);
    if (!ok) {
        return tl::make_unexpected(std::move(output.session->getErrors()));
    }

    // no errors and empty output could mean that the source is already normalized
    // so use input as output and lets the next stage try to figure out
    if (output.gnuCppSrc.empty()) {
        output.gnuCppSrc = std::move(input_file);
    }

#ifdef NORMALIZER_DEBUG_LOG
    llvm::outs() << "stage 1 GNU cpp source:\n\n" << output.gnuCppSrc << '\n';
#endif

    return output;
}
}  // namespace oklt