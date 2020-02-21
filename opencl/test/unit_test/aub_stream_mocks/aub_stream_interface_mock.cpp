/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_aub_manager.h"

namespace NEO {
aub_stream::AubManager *createAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, uint32_t streamMode) {
    return new MockAubManager(productFamily, devicesCount, memoryBankSize, localMemorySupported, streamMode);
}
} // namespace NEO