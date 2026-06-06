# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.

set(HYBM_KERNEL_PACK_SIGN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(HYBM_KERNEL_PROJECT_ROOT "${PROJECT_SOURCE_DIR}")

function(pack_targets_and_files)
    cmake_parse_arguments(ARG
        ""
        "OUTPUT;MANIFEST;OUTPUT_TARGET"
        "TARGETS;FILES"
        ${ARGN}
    )

    if(NOT ARG_OUTPUT)
        message(FATAL_ERROR "[pack_targets_and_files] OUTPUT is required")
    endif()
    if(NOT IS_ABSOLUTE "${ARG_OUTPUT}")
        set(ARG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${ARG_OUTPUT}")
    endif()
    if(NOT ARG_OUTPUT_TARGET)
        message(FATAL_ERROR "[pack_targets_and_files] OUTPUT_TARGET is required")
    endif()

    get_filename_component(tar_basename "${ARG_OUTPUT}" NAME_WE)
    string(MAKE_C_IDENTIFIER "pack_${tar_basename}" safe_name)
    set(staging_dir "${CMAKE_CURRENT_BINARY_DIR}/_${safe_name}_stage")

    set(src_items "")
    foreach(tgt IN LISTS ARG_TARGETS)
        if(NOT TARGET ${tgt})
            message(FATAL_ERROR "[pack_targets_and_files] Target '${tgt}' does not exist")
        endif()

        get_target_property(type ${tgt} TYPE)
        if(type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|STATIC_LIBRARY)$")
            list(APPEND src_items "$<TARGET_FILE:${tgt}>")
        endif()
    endforeach()
    list(APPEND src_items ${ARG_FILES})

    if(NOT src_items)
        message(FATAL_ERROR "[pack_targets_and_files] No targets or files specified to pack")
    endif()

    set(manifest_arg "")
    if(ARG_MANIFEST)
        if("${ARG_MANIFEST}" STREQUAL "")
            message(FATAL_ERROR "[pack_targets_and_files] MANIFEST filename cannot be empty")
        endif()
        if(IS_ABSOLUTE "${ARG_MANIFEST}")
            message(FATAL_ERROR "[pack_targets_and_files] MANIFEST must be relative")
        endif()
        set(manifest_arg -D_MANIFEST_FILE=${staging_dir}/${ARG_MANIFEST})
    endif()

    add_custom_command(
        OUTPUT ${staging_dir}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${staging_dir}"
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND ${CMAKE_COMMAND}
            -D _STAGING_DIR=${staging_dir}
            ${manifest_arg}
            -D "_ITEMS=$<JOIN:${src_items},;>"
            -P "${HYBM_KERNEL_PACK_SIGN_CMAKE_DIR}/hybm_pack_stage.cmake"
        COMMAND tar "czf" "${ARG_OUTPUT}" . --transform "s/^/aicpu_kernels_device\\//" "--mode=750"
        WORKING_DIRECTORY ${staging_dir}
        DEPENDS ${ARG_TARGETS} ${staging_dir}
        COMMENT "Packing ${ARG_OUTPUT}"
        VERBATIM
    )

    add_custom_target(${ARG_OUTPUT_TARGET} ALL DEPENDS "${ARG_OUTPUT}")
endfunction()
