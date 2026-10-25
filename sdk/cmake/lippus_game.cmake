# Aggiunge un gioco freestanding che produce <nome>.lbg nel build.
#
#   lippus_add_game(palla DISPLAY_NAME "Palla" SOURCES games/palla/main.c)

set(LIPPUS_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(LIPPUS_PACKER "${LIPPUS_SDK_DIR}/tools/pack_lbg.py")
set(LIPPUS_GAME_LD "${LIPPUS_SDK_DIR}/linker/game.ld")

if (NOT CMAKE_OBJCOPY)
    set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
endif()

function(lippus_add_game TARGET)
    cmake_parse_arguments(GAME "" "DISPLAY_NAME" "SOURCES" ${ARGN})
    if (NOT GAME_SOURCES)
        message(FATAL_ERROR "lippus_add_game(${TARGET}) vuole SOURCES")
    endif()
    if (NOT GAME_DISPLAY_NAME)
        set(GAME_DISPLAY_NAME "${TARGET}")
    endif()

    add_executable(${TARGET}
        ${GAME_SOURCES}
        ${LIPPUS_SDK_DIR}/src/crt0.S
        ${LIPPUS_SDK_DIR}/src/runtime.c
        ${LIPPUS_SDK_DIR}/src/orientation.c
    )
    target_include_directories(${TARGET} PRIVATE ${LIPPUS_SDK_DIR}/include)
    target_compile_options(${TARGET} PRIVATE
        -mcpu=cortex-m0plus
        -mthumb
        -Os
        -ffreestanding
        -fno-builtin
        -ffunction-sections
        -fdata-sections
        -fno-exceptions
        -Wall
        -Wextra
    )
    target_link_options(${TARGET} PRIVATE
        -nostdlib
        -nostartfiles
        -Wl,--gc-sections
        -Wl,-T,${LIPPUS_GAME_LD}
        -Wl,-e,lippus_game_entry
        -Wl,--no-warn-rwx-segments
    )
    target_link_libraries(${TARGET} PRIVATE gcc)
    set_target_properties(${TARGET} PROPERTIES LINK_DEPENDS ${LIPPUS_GAME_LD})

    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:${TARGET}>
                $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
        COMMAND python3 ${LIPPUS_PACKER}
                --elf $<TARGET_FILE:${TARGET}>
                --bin $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.bin
                --name "${GAME_DISPLAY_NAME}"
                --expect-load 0x10040000
                --out $<TARGET_FILE_DIR:${TARGET}>/${TARGET}.lbg
        VERBATIM
    )
endfunction()
