/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>

#include "WebrogueVirtGpu.h"
#include "Sync.h"
#include "util/log.h"

WebrogueVirtGpuDevice::WebrogueVirtGpuDevice(enum VirtGpuCapset capset)
    : VirtGpuDevice(capset) {
    memset(&mCaps, 0, sizeof(struct VirtGpuCaps));

    // Hard-coded values that may be assumed on Webrogue.
    mCaps.params[kParam3D] = 0;
    mCaps.params[kParamCapsetFix] = 0;
    mCaps.params[kParamResourceBlob] = 0;
    mCaps.params[kParamHostVisible] = 0;
    mCaps.params[kParamCrossDevice] = 0;
    mCaps.params[kParamContextInit] = 0;
    mCaps.params[kParamSupportedCapsetIds] = 0;
    mCaps.params[kParamExplicitDebugName] = 0;
    mCaps.params[kParamCreateGuestHandle] = 1;

    mCaps.vulkanCapset.protocolVersion = 1;
    mCaps.vulkanCapset.deferredMapping = 1;
    mCaps.vulkanCapset.blobAlignment = 16 * 1024;

    assert(capset == kCapsetGfxStreamVulkan);
    // if (capset == kCapsetGfxStreamVulkan) {
    //     uint64_t query_id = kMagmaVirtioGpuQueryCapset;
    //     query_id |= static_cast<uint64_t>(kCapsetGfxStreamVulkan) << 32;
    //     constexpr uint16_t kVersion = 0;
    //     query_id |= static_cast<uint64_t>(kVersion) << 16;

    //     magma_handle_t buffer;
    //     magma_status_t status = magma_device_query(device_, query_id, &buffer, nullptr);
    //     if (status == MAGMA_STATUS_OK) {
    //         zx::vmo capset_info(buffer);
    //         zx_status_t status =
    //             capset_info.read(&mCaps.vulkanCapset, /*offset=*/0, sizeof(struct vulkanCapset));
    //         mesa_logi("Got capset result, read status %d", status);
    //     } else {
    //         mesa_loge("Query(%lu) failed: status %d, expected buffer result", query_id, status);
    //     }

    //     // We always need an ASG blob in some cases, so always define blobAlignment
    //     if (!mCaps.vulkanCapset.blobAlignment) {
    //         mCaps.vulkanCapset.blobAlignment = 4096;
    //     }
    // }
}

WebrogueVirtGpuDevice::~WebrogueVirtGpuDevice() { }

int64_t WebrogueVirtGpuDevice::getDeviceHandle(void) { abort(); }

VirtGpuResourcePtr WebrogueVirtGpuDevice::createBlob(const struct VirtGpuCreateBlob& blobCreate) {
    assert(blobCreate.blobMem == kBlobMemGuest);
    return std::make_shared<WebrogueVirtGpuResource>(blobCreate.blobId, blobCreate.size);
}

VirtGpuResourcePtr WebrogueVirtGpuDevice::createResource(uint32_t width, uint32_t height,
                                                        uint32_t stride, uint32_t size,
                                                        uint32_t virglFormat, uint32_t target,
                                                        uint32_t bind) {
    abort();
}

VirtGpuResourcePtr WebrogueVirtGpuDevice::importBlob(const struct VirtGpuExternalHandle& handle) {
    abort();
}

int WebrogueVirtGpuDevice::execBuffer(struct VirtGpuExecBuffer& execbuffer,
                                     const VirtGpuResource* blob) {
    uint32_t hdr = *((uint32_t*)execbuffer.command);
    switch (hdr) {
        case GFXSTREAM_PLACEHOLDER_COMMAND_VK:
            return 0;
        default:
            abort();
    }
}

struct VirtGpuCaps WebrogueVirtGpuDevice::getCaps(void) { return mCaps; }

VirtGpuDevice* osCreateVirtGpuDevice(enum VirtGpuCapset capset, int32_t descriptor) {
    assert(descriptor < 0);
    return new WebrogueVirtGpuDevice(capset);
}

namespace gfxstream {

SyncHelper* osCreateSyncHelper() { return nullptr; }

}  // namespace gfxstream
