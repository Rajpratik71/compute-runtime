/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/source_level_debugger/source_level_debugger.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_builtins.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include <memory>

class CommandStreamReceiverWithActiveDebuggerTest : public ::testing::Test {
  protected:
    template <typename FamilyType>
    auto createCSR() {
        hwInfo = nullptr;
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<MockCsrHw2<FamilyType>>();
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo, 1);
        hwInfo->capabilityTable = platformDevices[0]->capabilityTable;
        hwInfo->capabilityTable.debuggerSupported = true;

        auto mockMemoryManager = new MockMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(mockMemoryManager);

        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, 0));
        device->setSourceLevelDebuggerActive(true);

        return static_cast<MockCsrHw2<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    }

    std::unique_ptr<MockClDevice> device;
    ExecutionEnvironment *executionEnvironment = nullptr;
    HardwareInfo *hwInfo = nullptr;
};

HWTEST_F(CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndDisabledPreemptionWhenFlushTaskIsCalledThenSipKernelIsMadeResident) {

    auto mockCsr = createCSR<FamilyType>();

    CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
    auto &commandStream = commandQueue.getCS(4096u);

    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

    void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

    std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
    std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

    mockCsr->flushTask(commandStream,
                       0,
                       *heap.get(),
                       *heap.get(),
                       *heap.get(),
                       0,
                       dispatchFlags,
                       device->getDevice());

    auto sipType = SipKernel::getSipKernelType(device->getHardwareInfo().platform.eRenderCoreFamily, true);
    auto sipAllocation = device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, device->getDevice()).getSipAllocation();
    bool found = false;
    for (auto allocation : mockCsr->copyOfAllocations) {
        if (allocation == sipAllocation) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    alignedFree(buffer);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndDisabledPreemptionWhenFlushTaskIsCalledThenStateSipCmdIsProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockCsr = createCSR<FamilyType>();

    if (device->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
        auto &commandStream = commandQueue.getCS(4096u);
        auto &preambleStream = mockCsr->getCS(0);

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        mockCsr->flushTask(commandStream,
                           0,
                           *heap.get(),
                           *heap.get(),
                           *heap.get(),
                           0,
                           dispatchFlags,
                           device->getDevice());

        auto sipType = SipKernel::getSipKernelType(device->getHardwareInfo().platform.eRenderCoreFamily, true);
        auto sipAllocation = device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, device->getDevice()).getSipAllocation();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(preambleStream);
        auto itorStateBaseAddr = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip);

        STATE_BASE_ADDRESS *sba = (STATE_BASE_ADDRESS *)*itorStateBaseAddr;
        STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
        EXPECT_LT(reinterpret_cast<void *>(sba), reinterpret_cast<void *>(stateSipCmd));

        auto sipAddress = stateSipCmd->getSystemInstructionPointer();

        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
        alignedFree(buffer);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandStreamReceiverWithActiveDebuggerTest, givenCsrWithActiveDebuggerAndWhenFlushTaskIsCalledThenAlwaysProgramStateBaseAddressAndSip) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto mockCsr = createCSR<FamilyType>();

    if (device->getHardwareInfo().capabilityTable.defaultPreemptionMode == PreemptionMode::MidThread) {
        mockCsr->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);

        CommandQueueHw<FamilyType> commandQueue(nullptr, device.get(), 0, false);
        auto &commandStream = commandQueue.getCS(4096u);
        auto &preambleStream = mockCsr->getCS(0);

        DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();

        void *buffer = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize64k);

        std::unique_ptr<MockGraphicsAllocation> allocation(new MockGraphicsAllocation(buffer, MemoryConstants::pageSize));
        std::unique_ptr<IndirectHeap> heap(new IndirectHeap(allocation.get()));

        mockCsr->flushTask(commandStream,
                           0,
                           *heap.get(),
                           *heap.get(),
                           *heap.get(),
                           0,
                           dispatchFlags,
                           device->getDevice());

        mockCsr->flushBatchedSubmissions();

        mockCsr->flushTask(commandStream,
                           0,
                           *heap.get(),
                           *heap.get(),
                           *heap.get(),
                           0,
                           dispatchFlags,
                           device->getDevice());

        auto sipType = SipKernel::getSipKernelType(device->getHardwareInfo().platform.eRenderCoreFamily, true);
        auto sipAllocation = device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, device->getDevice()).getSipAllocation();

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(preambleStream);
        auto itorStateBaseAddr = find<STATE_BASE_ADDRESS *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip);

        auto itorStateBaseAddr2 = find<STATE_BASE_ADDRESS *>(std::next(itorStateBaseAddr), hwParser.cmdList.end());
        auto itorStateSip2 = find<STATE_SIP *>(std::next(itorStateSip), hwParser.cmdList.end());

        ASSERT_NE(hwParser.cmdList.end(), itorStateBaseAddr2);
        ASSERT_NE(hwParser.cmdList.end(), itorStateSip2);

        STATE_BASE_ADDRESS *sba = (STATE_BASE_ADDRESS *)*itorStateBaseAddr2;
        STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip2;
        EXPECT_LT(reinterpret_cast<void *>(sba), reinterpret_cast<void *>(stateSipCmd));

        auto sipAddress = stateSipCmd->getSystemInstructionPointer();

        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress);
        alignedFree(buffer);
    }
}
