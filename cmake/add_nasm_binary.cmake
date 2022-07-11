# SPDX-License-Identifier: GPL-3.0-or-later

find_program(NASM NAMES ${CUSTOM_NASM_PATH} nasm REQUIRED)
add_executable(mos::nasm IMPORTED)
set_target_properties(mos::nasm PROPERTIES IMPORTED_LOCATION ${NASM})
message(STATUS "Found NASM at: ${NASM}")

if(NOT NASM)
    message(FATAL_ERROR
        " \n"
        " NASM is not available.\n"
        " \n"
        " Please install NASM and try again.\n"
        " Don't forget to configure the path to NASM in 'config.cmake'. \n"
        " \n")
endif()

function(add_nasm_binary TARGET)
    message(STATUS "Adding a new NASM binary target: ${TARGET}")
    if(TARGET ${TARGET})
        message(FATAL_ERROR
            " \n"
            " There has already been a target named:\n"
            "     '${TARGET}'\n"
            " \n"
            " Please suggest a NEW target name.\n"
            " \n")
    endif()

    set(options         ELF_OBJECT)
    set(single_val_arg  SOURCE)
    set(multi_val_arg)
    cmake_parse_arguments(ASSEMBLY_NEW_TARGET "${options}" "${single_val_arg}" "${multi_val_arg}" ${ARGN})
    if(ASSEMBLY_NEW_TARGET_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            " \n"
            " The following arguments cannot be understood:\n"
            "     ${ASSEMBLY_NEW_TARGET_UNPARSED_ARGUMENTS}\n"
            " \n"
            " Please check the arguments and try again.\n"
            " \n")
    endif()

    # If someone passed a relative path, make it absolute first.
    get_filename_component(ASSEMBLY_NEW_TARGET_SOURCE ${ASSEMBLY_NEW_TARGET_SOURCE} ABSOLUTE)

    # object_name: foo
    get_filename_component(object_name ${ASSEMBLY_NEW_TARGET_SOURCE} NAME_WLE)

    if(ASSEMBLY_NEW_TARGET_ELF_OBJECT)
        # to indicate that this file is an object, not a "executable binary"
        set(object_name "${object_name}.o")
        set(ASSEMBLY_NEW_TARGET_FLAGS -f elf)
    endif()

    # relative_source_file: ./path/to/foo.asm
    file(RELATIVE_PATH relative_source_file ${CMAKE_SOURCE_DIR} ${ASSEMBLY_NEW_TARGET_SOURCE})

    # relative_source_dir: ./path/to/
    get_filename_component(relative_source_dir ${relative_source_file} DIRECTORY)

    message(STATUS "  ${TARGET}: ${relative_source_file} ==NASM=>> ${relative_source_dir}/${object_name}")

    add_custom_target(${TARGET} COMMENT "NASM assembly target" SOURCES ${ASSEMBLY_NEW_TARGET_SOURCE})
    add_custom_command(
        TARGET ${TARGET}
        COMMENT "Assembling ${TARGET} with NASM (${NASM})..."
        VERBATIM
        COMMAND
            mos::nasm
                ${ASSEMBLY_NEW_TARGET_SOURCE}
                ${ASSEMBLY_NEW_TARGET_FLAGS}
                -o ${CMAKE_CURRENT_BINARY_DIR}/${relative_source_dir}/${object_name}
                -I ${CMAKE_CURRENT_SOURCE_DIR}/${relative_source_dir}
    )

    if(ASSEMBLY_NEW_TARGET_ELF_OBJECT)
        set(target_kind object)
    else()
        set(target_kind binary)
    endif()

    add_library(${TARGET}::${target_kind} SHARED IMPORTED)
    set_target_properties(${TARGET}::${target_kind} PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/${relative_source_dir}/${object_name})

    # small ninja hack
    set_target_properties(${TARGET} PROPERTIES EchoString "Assembling ${TARGET} with NASM (${NASM})...")

    # make directory: BUILDDIR/path/to/
    make_directory(${CMAKE_CURRENT_BINARY_DIR}/${relative_source_dir})
endfunction()
