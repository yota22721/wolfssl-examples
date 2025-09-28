
set(WOLFSSH_ROOT $ENV{WOLFSSH_ROOT})

file(GLOB FATFS_SRC
    "${WOLFSSH_ROOT}/ide/Linux-FATFS/*.c"
)

# FatFSライブラリを作成
add_library(fatfs STATIC
    ${FATFS_SRC}
)

# FatFSのインクルードディレクトリを設定
target_include_directories(fatfs PUBLIC
    ${WOLFSSH_ROOT}/ide/Linux-FATFS
)
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_sync/include)
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_base/include)
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_sync_spin_lock/include)
target_compile_definitions(fatfs PUBLIC
    #WOLFSSL_USER_SETTINGS
    #pico_stdlib
    #pico_cyw43_arch_lwip_sys_freertos
    #FreeRTOS-Kernel-Heap4
    RPI_PICO
    #PICO_RP2040
)
target_link_libraries(fatfs PUBLIC
    pico_stdlib
    hardware_flash
    pico_flash
    #pico_cyw43_arch_lwip_sys_freertos
    #FreeRTOS-Kernel-Heap4
)
#file(GLOB FATFS_EXCLUDE
#    "${WOLFSSH_ROOT}/ide/Linux-FATFS/fatfs_example.c"
#)
#foreach(FATFS_EXCLUDE ${FATFS_EXCLUDE})
#    list(REMOVE_ITEM FATFS_SRC ${FATFS_EXCLUDE})
#endforeach()
# ## wolfssh library
file(GLOB WOLFSSH_SRC
    "${WOLFSSH_ROOT}/src/*.c"
    "src/time.c"
)

file(GLOB WOLFSSH_EXCLUDE
    "${WOLFSSH_ROOT}/src/wolfsctp.c"
    "${WOLFSSH_ROOT}/src/certman.c"
    "${WOLFSSH_ROOT}/src/misc.c"
)

foreach(WOLFSSH_EXCLUDE ${WOLFSSH_EXCLUDE})
    list(REMOVE_ITEM WOLFSSH_SRC ${WOLFSSH_EXCLUDE})
endforeach()

add_library(wolfssh STATIC
    ${WOLFSSH_SRC}
)

#add_library(libwolfssl STATIC IMPORTED)
#set_target_properties(libwolfssl PROPERTIES
#    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libwolfssl.a"
#)

include_directories(${WOLFSSH_ROOT})
include_directories(${WOLFSSL_ROOT})
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_sync/include)
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_base/include)
#include_directories(${PICO_SDK_PATH}/src/rp2_common/hardware_sync_spin_lock/include)
#include_directories(${PICO_SDK_PATH}/src/rp2350/hardware_structs/include)
#include_directories(${PICO_SDK_PATH}/src/rp2350/hardware_regs/include)
#include_directories(${PICO_SDK_PATH}/src/common/pico_stdlib_headers/include)
target_compile_definitions(wolfssh PUBLIC
    WOLFSSL_USER_SETTINGS
    RPI_PICO
    #PICO_RP2040
    DATETIME=\"${DATETIME}\"
    STDIN_FILENO=0
    PRINTF=printf
    #WOLFSSH_SFTP
    #WOLFSSH_FATFS
)

if (${PICO_PLATFORM} STREQUAL "rp2350-arm-s")
add_compile_definitions(wolfssh WOLFSSL_SP_ARM_CORTEX_M_ASM)
elseif (${PICO_PLATFORM} STREQUAL "rp2350-riscv")
add_compile_definitions(wolfSSH WOLFSSL_SP_RISCV32)
else()
add_compile_definitions(wolfssh WOLFSSL_SP_ARM_THUMB_ASM)
endif()

target_link_libraries(wolfssh PUBLIC fatfs wolfssl)
### End of wolfssh library
