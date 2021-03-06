/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/scheduler/scheduler_kernel.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_ostime.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "test.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

using namespace NEO;
using namespace std;

class MockSchedulerKernel : public SchedulerKernel {
  public:
    MockSchedulerKernel(Program *program, const KernelInfo &info, const ClDevice &device) : SchedulerKernel(program, info, device) {
    }

    static MockSchedulerKernel *create(Program &program, Device &device, KernelInfo *&info) {
        info = new KernelInfo;
        SPatchDataParameterStream dataParametrStream;
        dataParametrStream.DataParameterStreamSize = 8;
        dataParametrStream.Size = 8;

        SPatchExecutionEnvironment executionEnvironment = {};
        executionEnvironment.CompiledSIMD32 = 1;
        executionEnvironment.HasDeviceEnqueue = 0;

        info->patchInfo.dataParameterStream = &dataParametrStream;
        info->patchInfo.executionEnvironment = &executionEnvironment;
        KernelArgInfo bufferArg;
        bufferArg.isBuffer = true;

        for (uint32_t i = 0; i < 9; i++) {
            bufferArg.kernelArgPatchInfoVector.resize(1);
            bufferArg.kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            bufferArg.kernelArgPatchInfoVector[0].size = 0;
            bufferArg.kernelArgPatchInfoVector[0].sourceOffset = 0;
            info->kernelArgInfo.push_back(std::move(bufferArg));
        }

        MockSchedulerKernel *mock = Kernel::create<MockSchedulerKernel>(&program, *info, nullptr);
        return mock;
    }
};

TEST(SchedulerKernelTest, getLws) {
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);

    size_t lws = kernel.getLws();
    EXPECT_EQ((size_t)24u, lws);
}

TEST(SchedulerKernelTest, getGws) {
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);

    const size_t hwThreads = 3;
    const size_t simdSize = 8;

    size_t maxGws = platformDevices[0]->gtSystemInfo.EUCount * hwThreads * simdSize;

    size_t gws = kernel.getGws();
    EXPECT_GE(maxGws, gws);
    EXPECT_LT(hwThreads * simdSize, gws);
}

TEST(SchedulerKernelTest, setGws) {
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);

    kernel.setGws(24);

    size_t gws = kernel.getGws();

    EXPECT_EQ(24u, gws);
}

TEST(SchedulerKernelTest, getCurbeSize) {
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;
    uint32_t crossTrheadDataSize = 32;
    uint32_t dshSize = 48;

    SPatchDataParameterStream dataParameterStream;
    dataParameterStream.DataParameterStreamSize = crossTrheadDataSize;

    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.DynamicStateHeapSize = dshSize;

    info.patchInfo.dataParameterStream = &dataParameterStream;
    info.heapInfo.pKernelHeader = &kernelHeader;

    MockSchedulerKernel kernel(&program, info, *device);

    uint32_t expectedCurbeSize = alignUp(crossTrheadDataSize, 64) + alignUp(dshSize, 64) + alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, setArgsForSchedulerKernel) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());
    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);
    unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get(),
                       allocs[8].get());

    for (uint32_t i = 0; i < 9; i++) {
        EXPECT_EQ(allocs[i].get(), scheduler->getKernelArg(i));
    }
}

TEST(SchedulerKernelTest, setArgsForSchedulerKernelWithNullDebugQueue) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());

    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);
    unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = unique_ptr<MockGraphicsAllocation>(new MockGraphicsAllocation((void *)0x1234, 10));
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get());

    for (uint32_t i = 0; i < 8; i++) {
        EXPECT_EQ(allocs[i].get(), scheduler->getKernelArg(i));
    }
    EXPECT_EQ(nullptr, scheduler->getKernelArg(8));
}

TEST(SchedulerKernelTest, givenGraphicsAllocationWithDifferentCpuAndGpuAddressesWhenCallSetArgsThenGpuAddressesAreTaken) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());

    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    auto scheduler = clUniquePtr(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);
    unique_ptr<MockGraphicsAllocation> allocs[9];

    for (uint32_t i = 0; i < 9; i++) {
        allocs[i] = std::make_unique<MockGraphicsAllocation>(reinterpret_cast<void *>(0x1234), 0x4321, 10);
    }

    scheduler->setArgs(allocs[0].get(),
                       allocs[1].get(),
                       allocs[2].get(),
                       allocs[3].get(),
                       allocs[4].get(),
                       allocs[5].get(),
                       allocs[6].get(),
                       allocs[7].get(),
                       allocs[8].get());

    for (uint32_t i = 0; i < 9; i++) {
        auto argAddr = reinterpret_cast<uint64_t>(scheduler->getKernelArgInfo(i).value);
        EXPECT_EQ(allocs[i]->getGpuAddress(), argAddr);
    }
}

TEST(SchedulerKernelTest, createKernelReflectionForForcedSchedulerDispatch) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());

    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    EXPECT_NE(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, createKernelReflectionSecondTimeForForcedSchedulerDispatchReturnsExistingAllocation) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(true);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());

    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    auto *allocation = scheduler->getKernelReflectionSurface();
    scheduler->createReflectionSurface();
    auto *allocation2 = scheduler->getKernelReflectionSurface();

    EXPECT_EQ(allocation, allocation2);
}

TEST(SchedulerKernelTest, createKernelReflectionForSchedulerDoesNothing) {
    DebugManagerStateRestore dbgRestorer;

    DebugManager.flags.ForceDispatchScheduler.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    program->setDevice(&device->getDevice());

    unique_ptr<KernelInfo> info(nullptr);
    KernelInfo *infoPtr = nullptr;
    unique_ptr<MockSchedulerKernel> scheduler = unique_ptr<MockSchedulerKernel>(MockSchedulerKernel::create(*program, device->getDevice(), infoPtr));
    info.reset(infoPtr);

    scheduler->createReflectionSurface();

    EXPECT_EQ(nullptr, scheduler->getKernelReflectionSurface());
}

TEST(SchedulerKernelTest, getCurbeSizeWithNullKernelInfo) {
    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;

    info.patchInfo.dataParameterStream = nullptr;
    info.heapInfo.pKernelHeader = nullptr;

    MockSchedulerKernel kernel(&program, info, *device);

    uint32_t expectedCurbeSize = alignUp(SCHEDULER_DYNAMIC_PAYLOAD_SIZE, 64);
    EXPECT_GE((size_t)expectedCurbeSize, kernel.getCurbeSize());
}

TEST(SchedulerKernelTest, givenForcedSchedulerGwsByDebugVariableWhenSchedulerKernelIsCreatedThenGwsIsSetToForcedValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SchedulerGWS.set(48);

    auto device = make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment());
    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);

    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(48u), gws);
}

TEST(SchedulerKernelTest, givenSimulationModeWhenSchedulerKernelIsCreatedThenGwsIsSetToOneWorkgroup) {
    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.featureTable.ftrSimulationMode = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockProgram program(*device->getExecutionEnvironment());

    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);
    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(24u), gws);
}

TEST(SchedulerKernelTest, givenForcedSchedulerGwsByDebugVariableAndSimulationModeWhenSchedulerKernelIsCreatedThenGwsIsSetToForcedValue) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.SchedulerGWS.set(48);

    HardwareInfo hwInfo = *platformDevices[0];
    hwInfo.featureTable.ftrSimulationMode = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    MockProgram program(*device->getExecutionEnvironment());

    KernelInfo info;
    MockSchedulerKernel kernel(&program, info, *device);
    size_t gws = kernel.getGws();
    EXPECT_EQ(static_cast<size_t>(48u), gws);
}
