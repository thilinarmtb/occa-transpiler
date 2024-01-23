#include "clang/Basic/DiagnosticSema.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "oklt/core/attribute_names.h"

namespace {

using namespace clang;
using namespace oklt;

constexpr ParsedAttrInfo::Spelling OUTER_ATTRIBUTE_SPELLINGS[] = {
  {ParsedAttr::AS_CXX11, "outer"},
  {ParsedAttr::AS_CXX11, OUTER_ATTR_NAME},
  {ParsedAttr::AS_GNU, "okl_outer"}};

struct OuterAttribute : public ParsedAttrInfo {
  OuterAttribute() {
    NumArgs = 0;
    OptArgs = 2;
    Spellings = OUTER_ATTRIBUTE_SPELLINGS;
    AttrKind = clang::AttributeCommonInfo::AT_Suppress;
    IsStmt = true;
  }

  bool diagAppertainsToDecl(clang::Sema& sema,
                            const clang::ParsedAttr& attr,
                            const clang::Decl* decl) const override {
    if (decl->getFriendObjectKind()) {
      sema.Diag(attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
        << attr << attr.isDeclspecAttribute() << "for statement";
      return false;
    }
    return true;
  }
};

ParsedAttrInfoRegistry::Add<OuterAttribute> register_okl_outer(OUTER_ATTR_NAME, "");
}  // namespace
