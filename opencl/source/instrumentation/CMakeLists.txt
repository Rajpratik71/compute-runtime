#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_INSTRUMENTATION
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/instrumentation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/instrumentation.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_INSTRUMENTATION})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_INSTRUMENTATION ${RUNTIME_SRCS_INSTRUMENTATION})
