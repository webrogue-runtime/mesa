/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <cstdlib>
#include "WebrogueVirtGpu.h"
#include "util/log.h"
#include <webroguegfx/webroguegfx.h>


WebrogueVirtGpuResource::WebrogueVirtGpuResource(uint64_t blobId, uint64_t size) {
    buf = aligned_alloc(16*1024, size);
    webroguegfx_vulkan_register_blob(blobId, size, buf);
}

WebrogueVirtGpuResource::~WebrogueVirtGpuResource(void) {}

uint32_t WebrogueVirtGpuResource::getBlobHandle() const {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}

uint32_t WebrogueVirtGpuResource::getResourceHandle() const {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}

uint64_t WebrogueVirtGpuResource::getSize() const {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}

VirtGpuResourceMappingPtr WebrogueVirtGpuResource::createMapping(void) {
    return std::make_shared<WebrogueVirtGpuResourceMapping>((uint8_t*)buf);
}

int WebrogueVirtGpuResource::wait() { return -1; }

int WebrogueVirtGpuResource::exportBlob(struct VirtGpuExternalHandle& handle) {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}

int WebrogueVirtGpuResource::transferFromHost(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}

int WebrogueVirtGpuResource::transferToHost(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    abort();
    mesa_loge("%s: unimplemented", __func__);
    return 0;
}
