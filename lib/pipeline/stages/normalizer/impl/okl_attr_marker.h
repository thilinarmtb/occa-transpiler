#pragma once

#include "pipeline/stages/normalizer/impl/okl_attribute.h"

#include <clang/Basic/SourceLocation.h>
#include <clang/AST/ParentMapContext.h>

using namespace clang;
namespace oklt {

struct OklAttrMarker {
    OklAttribute attr;
    struct {
        uint32_t line;
        uint32_t col;
    } loc;
};

}  // namespace oklt
