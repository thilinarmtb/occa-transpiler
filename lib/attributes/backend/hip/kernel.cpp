#include "attributes/attribute_names.h"
#include "attributes/utils/cuda_subset/handle.h"
#include "core/attribute_manager/attribute_manager.h"

namespace {
using namespace oklt;

__attribute__((constructor)) void registerKernelHandler() {
    auto ok = oklt::AttributeManager::instance().registerBackendHandler(
        {TargetBackend::HIP, KERNEL_ATTR_NAME},
        AttrDeclHandler{cuda_subset::handleKernelAttribute});

    if (!ok) {
        llvm::errs() << "failed to register " << KERNEL_ATTR_NAME << " attribute handler (CUDA)\n";
    }
}
}  // namespace
