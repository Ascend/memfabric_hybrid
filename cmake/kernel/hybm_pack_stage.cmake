# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

cmake_minimum_required(VERSION 3.12)

if(NOT _STAGING_DIR)
    message(FATAL_ERROR "_STAGING_DIR not set")
endif()
if(NOT _ITEMS)
    message(FATAL_ERROR "_ITEMS not set")
endif()

file(MAKE_DIRECTORY "${_STAGING_DIR}")

if(_MANIFEST_FILE)
    file(WRITE "${_MANIFEST_FILE}" "")
endif()

set(failed 0)
foreach(full_path IN LISTS _ITEMS)
    string(STRIP "${full_path}" full_path)
    if("${full_path}" MATCHES "^\\$")
        message(FATAL_ERROR "Generator expression not expanded: '${full_path}'")
    endif()

    if(NOT EXISTS "${full_path}")
        message(WARNING "Skip missing: '${full_path}'")
        math(EXPR failed "${failed} + 1")
        continue()
    endif()

    get_filename_component(basename "${full_path}" NAME)
    if("${basename}" STREQUAL "")
        message(WARNING "Skip empty basename: '${full_path}'")
        math(EXPR failed "${failed} + 1")
        continue()
    endif()

    file(COPY "${full_path}" DESTINATION "${_STAGING_DIR}")
    if(_MANIFEST_FILE)
        file(SHA256 "${full_path}" sha256)
        file(APPEND "${_MANIFEST_FILE}" "${basename}=${sha256}\n")
    endif()
endforeach()

if(failed GREATER 0)
    message(FATAL_ERROR "${failed} file(s) missing")
endif()
