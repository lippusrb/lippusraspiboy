cmake_minimum_required(VERSION 3.13)

# Pico classico, RP2040, 2 MB di flash. Il firmware occupa i primi 256 KB;
# da 0x10040000 in poi c'e' lo slot in cui vengono copiati i giochi della SD.
set(PICO_BOARD pico CACHE STRING "Board")

include(pico_sdk_import.cmake)

project(lippusboy C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

pico_sdk_init()

execute_process(
    COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/check_layout.py
    RESULT_VARIABLE LIPPUS_LAYOUT_OK
)
if (NOT LIPPUS_LAYOUT_OK EQUAL 0)
    message(FATAL_ERROR "sdk/include/lippus/layout.h non e' allineato al linker")
endif()

add_executable(lippus_os
    src/main.c
    src/log.c
    src/api.c
    src/gfx.c
    src/font.c
    src/pad.c
    src/audio.c
    src/sd.c
    src/fat.c
    src/catalog.c
    src/loader.c
    src/menu.c
)

target_include_directories(lippus_os PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/sdk/include
)

target_link_libraries(lippus_os PRIVATE
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_gpio
    hardware_flash
    hardware_sync
    pico_multicore
)

target_compile_definitions(lippus_os PRIVATE
    PICO_STACK_SIZE=0x1000
    PICO_HEAP_SIZE=0
)

# Lascia liberi gli ultimi 78 KB di SRAM per i dati dei giochi.
# Dal Pico SDK 2.2 la regione RAM e' fissa a 256k nello script di default:
# usiamo memmap_lippus.ld con LENGTH 0x2D800. I vecchi --defsym=RAM_LENGTH
# non bastano piu'.
pico_set_linker_script(lippus_os ${CMAKE_CURRENT_LIST_DIR}/src/memmap_lippus.ld)
target_link_options(lippus_os PRIVATE
    "LINKER:--print-memory-usage"
)

target_compile_options(lippus_os PRIVATE -Wall -Wextra)

pico_enable_stdio_usb(lippus_os 0)
pico_enable_stdio_uart(lippus_os 0)
pico_add_extra_outputs(lippus_os)

add_custom_command(TARGET lippus_os POST_BUILD
    COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/tools/check_os_size.py
            $<TARGET_FILE_DIR:lippus_os>/lippus_os.bin 0x40000
    VERBATIM
)

include(sdk/cmake/lippus_game.cmake)
lippus_add_game(palla DISPLAY_NAME "Palla" SOURCES games/palla/main.c)
lippus_add_game(tasti DISPLAY_NAME "Tasti" SOURCES games/tasti/main.c)
lippus_add_game(pacman DISPLAY_NAME "Pacman" SOURCES games/pacman/main.c)
lippus_add_game(maze_mouse DISPLAY_NAME "Maze Mouse" SOURCES games/maze_mouse/main.c)
lippus_add_game(garden_defender DISPLAY_NAME "Garden Defender" SOURCES games/garden_defender/main.c)
lippus_add_game(lippus_bounce DISPLAY_NAME "Lippus Bounce" SOURCES games/lippus_bounce/main.c)
lippus_add_game(lippus_breaker DISPLAY_NAME "Lippus Breaker" SOURCES games/lippus_breaker/main.c)
