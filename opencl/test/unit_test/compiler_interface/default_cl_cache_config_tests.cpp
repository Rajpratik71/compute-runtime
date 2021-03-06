/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/compiler_interface/default_cl_cache_config.h"
#include "test.h"

TEST(CompilerCache, GivenDefaultClCacheConfigThenValuesAreProperlyPopulated) {
    auto cacheConfig = NEO::getDefaultClCompilerCacheConfig();
    EXPECT_STREQ("cl_cache", cacheConfig.cacheDir.c_str());
    EXPECT_STREQ(".cl_cache", cacheConfig.cacheFileExtension.c_str());
    EXPECT_TRUE(cacheConfig.enabled);
}
