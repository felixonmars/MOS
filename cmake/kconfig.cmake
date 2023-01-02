# SPDX-License-Identifier: GPL-3.0-or-later
set(MOS_KCONFIG_DEFINES)
set(MOS_SUMMARY_NAME_LENGTH 30)
set(MOS_SUMMARY_DESCRIPTION_LENGTH 40)

macro(mos_kconfig SECTION NAME DEFAULT HELP)
    set(TYPE "STRING")

    if(DEFAULT STREQUAL "ON" OR DEFAULT STREQUAL "OFF" OR DEFAULT STREQUAL "YES" OR DEFAULT STREQUAL "NO" OR DEFAULT STREQUAL "TRUE" OR DEFAULT STREQUAL "FALSE")
        set(TYPE "BOOLEAN")
    endif()

    set(${NAME} ${DEFAULT} CACHE ${TYPE} "${HELP}")
    set(_value "${${NAME}}")
    mos_add_summary_item(${SECTION} "${NAME}" "${HELP}" "${_value}")

    list(APPEND MOS_KCONFIG_DEFINES "${NAME}")
    set(_MOS_KCONFIG_DEFINES_${NAME}_file "${CMAKE_CURRENT_LIST_FILE}")
endmacro()

function(generate_kconfig TARGET)
    message(STATUS "Generating kconfig.c according to configuration...")

    make_directory(${CMAKE_BINARY_DIR}/include)

    set(KCONFIG_H "${CMAKE_BINARY_DIR}/include/mos/kconfig.h")
    file(WRITE ${KCONFIG_H} "// SPDX-License-Identifier: GPL-3.0-or-later\n")
    file(APPEND ${KCONFIG_H} "\n")
    file(APPEND ${KCONFIG_H} "#pragma once\n")
    file(APPEND ${KCONFIG_H} "\n")
    file(APPEND ${KCONFIG_H} "// ! This file is generated by CMake. It's not intended for manual editing.\n")
    file(APPEND ${KCONFIG_H} "// !! All changes to this file will be lost.\n")
    file(APPEND ${KCONFIG_H} "\n")

    configure_file(${CMAKE_SOURCE_DIR}/cmake/kconfig.h.in "${KCONFIG_H}")
    target_sources(${TARGET} PRIVATE "${KCONFIG_H}")

    set(_prev_file "")

    foreach(CONFIG ${MOS_KCONFIG_DEFINES})
        set(_file "${_MOS_KCONFIG_DEFINES_${CONFIG}_file}")
        set(_val "${${CONFIG}}")

        if(_val STREQUAL "ON" OR _val STREQUAL "YES" OR _val STREQUAL "TRUE" OR _val STREQUAL "1")
            set(_val 1)
        elseif(_val STREQUAL "OFF" OR _val STREQUAL "NO" OR _val STREQUAL "FALSE" OR _val STREQUAL "0")
            set(_val -1)
        endif()

        if(NOT _file STREQUAL _prev_file)
            file(APPEND ${KCONFIG_H} "\n// defined in ${_file}\n")
            set(_prev_file "${_file}")
        endif()

        file(APPEND ${KCONFIG_H} "#define ${CONFIG} ${_val}\n")
    endforeach()
endfunction()

macro(summary_section section name)
    if(DEFINED MOS_SUMMARY_SECTION_${section})
        message(FATAL_ERROR "ERROR: summary section ${section} already defined")
    endif()

    set(MOS_SUMMARY_SECTION_${section} "${name}")
    list(APPEND MOS_SUMMARY_SECTION_ORDER ${section})

    get_directory_property(hasParent PARENT_DIRECTORY)

    if(hasParent)
        set(MOS_SUMMARY_SECTION_ORDER "${MOS_SUMMARY_SECTION_ORDER}" PARENT_SCOPE)
        set(MOS_SUMMARY_SECTION_${section} "${name}" PARENT_SCOPE)
    endif()
endmacro()

macro(mos_add_summary_item section name description value)
    if(NOT DEFINED MOS_SUMMARY_SECTION_${section})
        message(FATAL_ERROR "Unknown summary section '${section}'")
    endif()

    string(LENGTH ${name} NAME_LEN)
    math(EXPR padding "${MOS_SUMMARY_NAME_LENGTH} - ${NAME_LEN} - 2")

    if(padding LESS 0)
        set(padding 0)
    endif()

    string(REPEAT "." ${padding} PADDING_STRING_NAME)

    string(LENGTH ${description} DESC_LEN)
    math(EXPR padding "${MOS_SUMMARY_DESCRIPTION_LENGTH} - ${DESC_LEN} - 2")

    if(padding LESS 0)
        set(padding 0)
    endif()

    string(REPEAT "." ${padding} PADDING_STRING_DESC)

    list(APPEND MOS_SUMMARY_SECTION_CONTENT_${section} "${description} ${PADDING_STRING_DESC}${PADDING_STRING_NAME} ${name} = ${value}")

    get_directory_property(hasParent PARENT_DIRECTORY)

    if(hasParent)
        set(MOS_SUMMARY_SECTION_CONTENT_${section} "${MOS_SUMMARY_SECTION_CONTENT_${section}}" PARENT_SCOPE)
    endif()
endmacro()

function(mos_print_summary)
    message("Configuration Summary:")

    foreach(section ${MOS_SUMMARY_SECTION_ORDER})
        message("  ${MOS_SUMMARY_SECTION_${section}}")

        foreach(item ${MOS_SUMMARY_SECTION_${section}})
            foreach(item ${MOS_SUMMARY_SECTION_CONTENT_${section}})
                message("    ${item}")
            endforeach()
        endforeach()

        message("")
    endforeach()

    mos_save_summary()
endfunction()

function(mos_save_summary)
    set(SUMMARY_FILE "${CMAKE_BINARY_DIR}/summary.txt")
    file(WRITE ${SUMMARY_FILE} "Configuration Summary:\n")

    foreach(section ${MOS_SUMMARY_SECTION_ORDER})
        file(APPEND ${SUMMARY_FILE} "  ${MOS_SUMMARY_SECTION_${section}}\n")

        foreach(item ${MOS_SUMMARY_SECTION_CONTENT_${section}})
            file(APPEND ${SUMMARY_FILE} "    ${item}\n")
        endforeach()

        file(APPEND ${SUMMARY_FILE} "\n")
    endforeach()
endfunction()
