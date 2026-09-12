set(WOLFIP_ROOT $ENV{WOLFIP_ROOT})

file(GLOB WOLFIP_SRC
    "${WOLFIP_ROOT}/src/*.c"
    "${WOLFIP_ROOT}/src/port/freeRTOS/*.c"
)

add_library(wolfip STATIC
    ${WOLFIP_SRC}
)

target_include_directories(wolfip PUBLIC
    #${CMAKE_CURRENT_LIST_DIR}/config
    ${WOLFIP_ROOT}/src
    ${WOLFIP_ROOT}/src/port/freeRTOS
    ${WOLFIP_ROOT}
)

target_compile_definitions(wolfip PUBLIC
    WOLFSSL_USER_SETTINGS
)

target_link_libraries(wolfip
    pico_stdlib
    FreeRTOS-Kernel
)