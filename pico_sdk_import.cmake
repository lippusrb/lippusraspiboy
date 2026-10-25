# Copia ridotta dello script di import del Pico SDK (BSD-3-Clause, Raspberry Pi).

if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("PICO_SDK_PATH = ${PICO_SDK_PATH}")
endif()

if (NOT PICO_SDK_PATH)
    message(FATAL_ERROR "Imposta PICO_SDK_PATH alla cartella del Pico SDK.")
endif()

get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" REALPATH BASE_DIR "${CMAKE_BINARY_DIR}")
if (NOT EXISTS ${PICO_SDK_PATH}/pico_sdk_init.cmake)
    message(FATAL_ERROR "Pico SDK non trovato in ${PICO_SDK_PATH}")
endif()

set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
include(${PICO_SDK_INIT_CMAKE_FILE})
