#include "pipeline/stages/normalizer/normalizer.h"
#include <oklt/core/error.h>

namespace oklt {
UserResult normalize(UserInput input) {
    return runNormalizerStage(TranspilerSession::make(std::move(input))).and_then(toUserResult);
}
}  // namespace oklt