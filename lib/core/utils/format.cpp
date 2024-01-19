#include <vector>
#include "clang/Format/Format.h"
#include "clang/Tooling/Core/Replacement.h"
#include "oklt/core/utils/format.h"

using namespace clang;
using namespace clang::tooling;

namespace oklt {
    std::string format(llvm::StringRef code) {
        const std::vector<Range> ranges(1, Range(0, code.size()));
        auto Style = format::getLLVMStyle();

        Replacements replaces = format::reformat(Style, code, ranges);
        auto changedCode = applyAllReplacements(code, replaces);
        if (!changedCode) {
            llvm::errs() << toString(changedCode.takeError());
            return {};
        }
        return changedCode.get();
    }
}