/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/options.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

class AubCenter;
class GmmClientContext;
class GmmHelper;
class ExecutionEnvironment;
class GmmPageTableMngr;
class MemoryOperationsHandler;
class OSInterface;
struct HardwareInfo;
class HwDeviceId;

struct RootDeviceEnvironment {
    RootDeviceEnvironment(ExecutionEnvironment &executionEnvironment);
    RootDeviceEnvironment(RootDeviceEnvironment &) = delete;
    MOCKABLE_VIRTUAL ~RootDeviceEnvironment();
    MOCKABLE_VIRTUAL const HardwareInfo *getHardwareInfo() const;

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    bool initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId);
    void initGmm();
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;

    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<GmmPageTableMngr> pageTableManager;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<AubCenter> aubCenter;
    ExecutionEnvironment &executionEnvironment;
};
} // namespace NEO
