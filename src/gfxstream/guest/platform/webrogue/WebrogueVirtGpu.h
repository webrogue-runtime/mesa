/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "VirtGpu.h"

class WebrogueVirtGpuResource : public VirtGpuResource {
   public:
    WebrogueVirtGpuResource(uint64_t blobId, uint64_t size);
    ~WebrogueVirtGpuResource();

    uint32_t getResourceHandle() const override;
    uint32_t getBlobHandle() const override;
    uint64_t getSize() const override;
    int wait() override;

    int exportBlob(struct VirtGpuExternalHandle& handle) override;
    int transferFromHost(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
    int transferToHost(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

    VirtGpuResourceMappingPtr createMapping(void) override;
   private:
    void* buf;
};

class WebrogueVirtGpuResourceMapping : public VirtGpuResourceMapping {
   public:
    WebrogueVirtGpuResourceMapping(uint8_t* ptr);
    ~WebrogueVirtGpuResourceMapping(void);

    uint8_t* asRawPtr(void) override;
   private:
    uint8_t* buf;
};

class WebrogueVirtGpuDevice : public VirtGpuDevice {
   public:
    WebrogueVirtGpuDevice(enum VirtGpuCapset capset);
    ~WebrogueVirtGpuDevice();

    int64_t getDeviceHandle(void) override;

    struct VirtGpuCaps getCaps(void) override;

    VirtGpuResourcePtr createBlob(const struct VirtGpuCreateBlob& blobCreate) override;
    VirtGpuResourcePtr createResource(uint32_t width, uint32_t height, uint32_t stride,
                                      uint32_t size, uint32_t virglFormat, uint32_t target,
                                      uint32_t bind) override;
    VirtGpuResourcePtr importBlob(const struct VirtGpuExternalHandle& handle) override;

    int execBuffer(struct VirtGpuExecBuffer& execbuffer, const VirtGpuResource* blob) override;

   private:
    struct VirtGpuCaps mCaps;
};
