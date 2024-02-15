#include <clang/AST/Attr.h>
#include <oklt/core/utils/attributes.h>
#include <oklt/core/transpiler_session/session_stage.h>

namespace oklt {
using namespace clang;

bool removeAttribute(const clang::Attr *attr, SessionStage &stage) {
  auto &rewriter = stage.getRewriter();
  auto range = getAttrFullSourceRange(*attr);
  //INFO: sometimes rewrite functions does the job but return false value
  rewriter.RemoveText(range);
  return true;
}

constexpr SourceLocation::IntTy CXX11_ATTR_PREFIX_LEN = std::char_traits<char>::length("[[");
constexpr SourceLocation::IntTy CXX11_ATTR_SUFFIX_LEN = std::char_traits<char>::length("]]");

constexpr SourceLocation::IntTy GNU_ATTR_PREFIX_LEN = std::char_traits<char>::length("__attribute__((");
constexpr SourceLocation::IntTy GNU_ATTR_SUFFIX_LEN = std::char_traits<char>::length("))");

SourceRange getAttrFullSourceRange(const Attr& attr) {
  auto arange = attr.getRange();

  if (attr.isCXX11Attribute() || attr.isC2xAttribute()) {
    arange.setBegin(arange.getBegin().getLocWithOffset(-CXX11_ATTR_PREFIX_LEN));
    arange.setEnd(arange.getEnd().getLocWithOffset(CXX11_ATTR_SUFFIX_LEN));
  }

  if (attr.isGNUAttribute()) {
    arange.setBegin(arange.getBegin().getLocWithOffset(-GNU_ATTR_PREFIX_LEN));
    arange.setEnd(arange.getEnd().getLocWithOffset(GNU_ATTR_SUFFIX_LEN));
  }

  return arange;
}

}
