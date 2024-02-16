#include <oklt/core/attribute_manager/attribute_manager.h>
#include <oklt/core/attribute_names.h>
#include <oklt/util/string_utils.h>

#include "attributes/params/tile.hpp"
#include "attributes/utils/parse.h"

#include "clang/Basic/DiagnosticSema.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"

namespace {

using namespace oklt;
using namespace clang;

constexpr ParsedAttrInfo::Spelling TILE_ATTRIBUTE_SPELLINGS[] = {
    {ParsedAttr::AS_CXX11, "tile"},
    {ParsedAttr::AS_CXX11, TILE_ATTR_NAME},
    {ParsedAttr::AS_GNU, "okl_tile"}};

struct TileAttribute : public ParsedAttrInfo {
    TileAttribute() {
        NumArgs = 1;
        OptArgs = 0;
        Spellings = TILE_ATTRIBUTE_SPELLINGS;
        AttrKind = clang::AttributeCommonInfo::AT_Suppress;
        IsStmt = true;
    }
    bool diagAppertainsToStmt(clang::Sema& sema,
                              const clang::ParsedAttr& attr,
                              const clang::Stmt* stmt) const override {
        if (!isa<ForStmt>(stmt)) {
            sema.Diag(attr.getLoc(), diag::err_attribute_wrong_decl_type_str)
                << attr << attr.isDeclspecAttribute() << "for statement";
            return false;
        }
        return true;
    }

    bool diagAppertainsToDecl(clang::Sema& sema,
                              const clang::ParsedAttr& attr,
                              const clang::Decl* decl) const override {
        // INFO: fail for all decls
        sema.Diag(attr.getLoc(), diag::err_attribute_wrong_decl_type_str)
            << attr << attr.isDeclspecAttribute() << "for statement";
        return false;
    }
};

constexpr int MAX_N_PARAMS = 4;

tl::expected<int, Error> parseTileSize(const std::string& str) {
    char* p;
    auto tileSize = strtol(str.c_str(), &p, 10);
    if (!p) {
        return tl::make_unexpected(Error{std::error_code(), "Tile size is not an integer"});
    }
    return tileSize;
}

tl::expected<LoopType, Error> parseLoopType(const std::string& str) {
    if (str == "@outer") {
        return LoopType::Outer;
    } else if (str == "@inner") {
        return LoopType::Inner;
    }
    return tl::make_unexpected(Error{std::error_code(), "Tile loop type parse error"});
}

tl::expected<bool, Error> parseCheck(const std::string& str) {
    constexpr const char* err_msg = "Tile check parameter format: 'check=true/false'";
    auto err = tl::make_unexpected(Error{std::error_code(), err_msg});
    auto pos = str.find("=");
    if (pos == std::string::npos || pos == (str.size() - 1)) {
        return err;
    }

    auto trueFalse = str.substr(pos + 1);
    if (trueFalse == "true") {
        return true;
    } else if (trueFalse == "false") {
        return false;
    }
    return err;
}

bool parseTileAttribute(const clang::Attr* a, SessionStage& s) {
    auto tileParamsStr = parseOKLAttributeParamsStr(a);
    if (!tileParamsStr.has_value()) {
        s.pushError(tileParamsStr.error());
        return false;
    }

    auto nParams = tileParamsStr->size();
    if (nParams > MAX_N_PARAMS || nParams < 1) {
        s.pushError(std::error_code(), "Tile has 1 to 4 parameters");
        return false;
    }

    // Parse all parameters:
    auto tileSize = parseTileSize(tileParamsStr.value()[0]);  // tileParamsStr is not empty for sure
    if (!tileSize.has_value()) {
        s.pushError(tileSize.error());
        return false;
    }

    std::vector<bool> checksStack;
    std::vector<LoopType> loopsStack;

    // TODO: rewrite this ugly parsing if possible
    for (int i = 1; i < nParams; ++i) {
        auto currentParamStr = tileParamsStr.value()[i];
        // Parse check
        if (auto check = parseCheck(currentParamStr)) {
            checksStack.push_back(check.value());
            continue;
        }

        // Parse loop
        if (auto loopType = parseLoopType(currentParamStr)) {
            loopsStack.push_back(loopType.value());
            continue;
        }

        s.pushError(std::error_code(), "Can't parse tile parameter: " + currentParamStr);
        return false;
    }

    // Verify number of parameters
    if (checksStack.size() > 1) {
        s.pushError(std::error_code(), "More than one tile check parameters");
        return false;
    }

    if (loopsStack.size() > 2) {
        s.pushError(std::error_code(), "More than two tile loop identifiers");
        return false;
    }

    TileParams tileParams{
        .tileSize = tileSize.value(),
        .firstLoopType = !loopsStack.empty() ? loopsStack[0] : LoopType::Regular,
        .secondLoopType = loopsStack.size() > 1 ? loopsStack[1] : LoopType::Regular,
        .check = !checksStack.empty() ? checksStack.front() : true,
    };

    // Outer can't be after inner:
    if (tileParams.firstLoopType == LoopType::Inner &&
        tileParams.secondLoopType == LoopType::Outer) {
        s.pushError(std::error_code(), "Cannot have [@inner] loop outside of an [@outer] loop");
        return false;
    }

    auto ctxKey = util::pointerToStr(static_cast<const void*>(a));
    s.tryEmplaceUserCtx<TileParams>(ctxKey, tileParams);

#ifdef TRANSPILER_DEBUG_LOG
    llvm::outs() << "[DEBUG] parsed tile parameters: " << ctxKey
                 << ": {tile size: " << tileParams.tileSize
                 << ", first loop: " << static_cast<int>(tileParams.firstLoopType)
                 << ", second loop: " << static_cast<int>(tileParams.secondLoopType)
                 << ", check: " << tileParams.check << "}\n";
#endif
    return true;
}

__attribute__((constructor)) void registerKernelHandler() {
    AttributeManager::instance().registerAttrFrontend<TileAttribute>(TILE_ATTR_NAME,
                                                                     parseTileAttribute);
}
}  // namespace
