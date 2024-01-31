#pragma once

#include <string>
#include <tl/expected.hpp>

namespace oklt {
struct TranspilerSession;
}

namespace oklt {
struct NormalizerError {};

struct NormalizerInput {
  std::string oklSource;
};

struct NormalizerOutput {
  std::string cppSource;
};

using ExpecteNormalizerResult = tl::expected<NormalizerOutput, NormalizerError>;

ExpecteNormalizerResult normalize(NormalizerInput input, TranspilerSession& session);
}  // namespace oklt
