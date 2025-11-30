/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "WebrogueVirtGpu.h"

WebrogueVirtGpuResourceMapping::WebrogueVirtGpuResourceMapping(uint8_t* ptr): buf(ptr) {}

WebrogueVirtGpuResourceMapping::~WebrogueVirtGpuResourceMapping(void) {}

uint8_t* WebrogueVirtGpuResourceMapping::asRawPtr(void) { return buf; }
