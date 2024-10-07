
set(WOLFSSH_ROOT $ENV{WOLFSSH_ROOT})

# ## wolfssh library
file(GLOB WOLFSSH_SRC
    "${WOLFSSH_ROOT}/src/*.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/*.c"
    "${WOLFSSL_ROOT}/src/*.c"
)

file(GLOB WOLFSSH_EXCLUDE
    "${WOLFSSH_ROOT}/src/wolfsctp.c"
    "${WOLFSSH_ROOT}/src/wolfsftp.c"
    "${WOLFSSH_ROOT}/src/certman.c"
    "${WOLFSSH_ROOT}/src/misc.c"
    "${WOLFSSL_ROOT}/src/bio.c"
    "${WOLFSSL_ROOT}/src/conf.c"
    "${WOLFSSL_ROOT}/src/pk.c"
    "${WOLFSSL_ROOT}/src/x509.c"
    "${WOLFSSL_ROOT}/src/ssl_*.c"
    "${WOLFSSL_ROOT}/src/x509_*.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/misc.c"
    "${WOLFSSL_ROOT}/wolfcrypt/src/evp.c"
)

foreach(WOLFSSH_EXCLUDE ${WOLFSSH_EXCLUDE})
    list(REMOVE_ITEM WOLFSSH_SRC ${WOLFSSH_EXCLUDE})
endforeach()

add_library(wolfssh STATIC
    ${WOLFSSH_SRC}
)


include_directories(${WOLFSSH_ROOT})

target_compile_definitions(wolfssh PUBLIC
    WOLFSSL_USER_SETTINGS
    RPI_PICO
)
### End of wolfssh library
