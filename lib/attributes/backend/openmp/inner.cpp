#include "attributes/backend/openmp/common.h"

#include <spdlog/spdlog.h>

namespace {
using namespace oklt;
using namespace clang;

__attribute__((constructor)) void registerOPENMPOuterHandler() {
    auto ok = registerBackendHandler(
        TargetBackend::OPENMP, INNER_ATTR_NAME, serial_subset::handleInnerAttribute);

    if (!ok) {
        SPDLOG_ERROR("[OPENMP] Failed to register {} attribute handler", INNER_ATTR_NAME);
    }
}
}  // namespace
