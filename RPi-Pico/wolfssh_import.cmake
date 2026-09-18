set(WOLFSSH_ROOT $ENV{WOLFSSH_ROOT})

set(WOLFSSH_SRC
    "${WOLFSSH_ROOT}/src/ssh.c"
    "${WOLFSSH_ROOT}/src/internal.c"
    "${WOLFSSH_ROOT}/src/log.c"
    "${WOLFSSH_ROOT}/src/io.c"
    "${WOLFSSH_ROOT}/src/port.c"
    "${WOLFSSH_ROOT}/src/ossh.c"
)

add_library(wolfssh STATIC ${WOLFSSH_SRC})

# wolfSSL and wolfSSH must see the same embedded/allocator settings.
target_compile_definitions(wolfssl PUBLIC WOLFSSH_PICO_BUILD)
target_link_libraries(wolfssl FreeRTOS-Kernel)

target_include_directories(wolfssh PUBLIC
    ${WOLFSSH_ROOT}
)

target_compile_definitions(wolfssh PUBLIC
    BUILDING_WOLFSSH
    WOLFSSL_USER_SETTINGS
    WOLFSSH_PICO_BUILD
)

target_link_libraries(wolfssh PUBLIC wolfssl)
