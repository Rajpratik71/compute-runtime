/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include "cif/common/cif_main.h"
#include "ocl_igc_interface/fcl_ocl_device_ctx.h"
#include "ocl_igc_interface/igc_ocl_device_ctx.h"

#include <cstdint>
#include <memory>
#include <string>

namespace NEO {

struct HardwareInfo;
class OsLibrary;

std::string convertToPascalCase(const std::string &inString);

enum ErrorCode {
    SUCCESS = 0,
    OUT_OF_HOST_MEMORY = -6,
    BUILD_PROGRAM_FAILURE = -11,
    INVALID_DEVICE = -33,
    INVALID_PROGRAM = -44,
    INVALID_COMMAND_LINE = -5150,
    INVALID_FILE = -5151,
    PRINT_USAGE = -5152,
};

std::string generateFilePath(const std::string &directory, const std::string &fileNameBase, const char *extension);
std::string getDevicesTypes();

class OfflineCompiler {
  public:
    static OfflineCompiler *create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal);
    static OfflineCompiler *create(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles, int &retVal, std::unique_ptr<OclocArgHelper> helper);
    int build();
    std::string &getBuildLog();
    void printUsage();

    OfflineCompiler &operator=(const OfflineCompiler &) = delete;
    OfflineCompiler(const OfflineCompiler &) = delete;
    ~OfflineCompiler();

    bool isQuiet() const {
        return quiet;
    }

    std::string parseBinAsCharArray(uint8_t *binary, size_t size, std::string &fileName);

    static bool readOptionsFromFile(std::string &optionsOut, const std::string &file, std::unique_ptr<OclocArgHelper> &helper);

    ArrayRef<const uint8_t> getPackedDeviceBinaryOutput() {
        return this->elfBinary;
    }

    static std::string getFileNameTrunk(std::string &filePath);
    const HardwareInfo &getHardwareInfo() const {
        return *hwInfo;
    }

  protected:
    OfflineCompiler();

    int getHardwareInfo(const char *pDeviceName);
    std::string getStringWithinDelimiters(const std::string &src);
    int initialize(size_t numArgs, const std::vector<std::string> &allArgs, bool dumpFiles);
    int parseCommandLine(size_t numArgs, const std::vector<std::string> &allArgs);
    void setStatelessToStatefullBufferOffsetFlag();
    void resolveExtraSettings();
    void parseDebugSettings();
    void storeBinary(char *&pDst, size_t &dstSize, const void *pSrc, const size_t srcSize);
    int buildSourceCode();
    void updateBuildLog(const char *pErrorString, const size_t errorStringSize);
    bool generateElfBinary();
    std::string generateFilePathForIr(const std::string &fileNameBase) {
        const char *ext = (isSpirV) ? ".spv" : ".bc";
        return generateFilePath(outputDirectory, fileNameBase, useLlvmText ? ".ll" : ext);
    }

    std::string generateOptsSuffix() {
        std::string suffix{useOptionsSuffix ? options : ""};
        std::replace(suffix.begin(), suffix.end(), ' ', '_');
        return suffix;
    }
    void writeOutAllFiles();
    const HardwareInfo *hwInfo = nullptr;

    std::string deviceName;
    std::string familyNameWithType;
    std::string inputFile;
    std::string outputFile;
    std::string outputDirectory;
    std::string options;
    std::string internalOptions;
    std::string sourceCode;
    std::string buildLog;
    bool dumpFiles = true;

    bool useLlvmText = false;
    bool useLlvmBc = false;
    bool useCppFile = false;
    bool useOptionsSuffix = false;
    bool quiet = false;
    bool inputFileLlvm = false;
    bool inputFileSpirV = false;
    bool outputNoSuffix = false;
    bool forceStatelessToStatefulOptimization = false;

    std::vector<uint8_t> elfBinary;
    char *genBinary = nullptr;
    size_t genBinarySize = 0;
    char *irBinary = nullptr;
    size_t irBinarySize = 0;
    bool isSpirV = false;
    char *debugDataBinary = nullptr;
    size_t debugDataBinarySize = 0;

    std::unique_ptr<OsLibrary> igcLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> igcMain = nullptr;
    CIF::RAII::UPtr_t<IGC::IgcOclDeviceCtxTagOCL> igcDeviceCtx = nullptr;

    std::unique_ptr<OsLibrary> fclLib = nullptr;
    CIF::RAII::UPtr_t<CIF::CIFMain> fclMain = nullptr;
    CIF::RAII::UPtr_t<IGC::FclOclDeviceCtxTagOCL> fclDeviceCtx = nullptr;
    IGC::CodeType::CodeType_t preferredIntermediateRepresentation;

    std::unique_ptr<OclocArgHelper> argHelper = nullptr;
};
} // namespace NEO