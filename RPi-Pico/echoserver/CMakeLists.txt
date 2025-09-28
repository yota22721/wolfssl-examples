add_executable(echoServer
    ../src/wolfssh_main.c
    ../src/wifi.c
    ../src/tcp.c
    ../src/freertos_cb.c
    ../src/time.c
)

target_compile_definitions(echoServer PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    TCP_SERVER=\"${TCP_SERVER}\"
    NO_SYS=0
    LWIP_SOCKET
    LWIP_COMPAT_SOCKETS
    PING_USE_SOCKETS=1
    WOLFSSL_USER_SETTINGS
    DATETIME=\"${DATETIME}\"
    RPI_PICO
    #define USE_RAM

    #define USE_FLASH
    #define NO_WOLFSSH_DIR
    #define WOLFSSH_SFTP_SETMODE
    #undef WOLFSSH_USER_FILESYSTEM
    #define WOLFSSH_SFTP_SETMODEHANDLE
    #define WOLFSSH_STOREHANDLE
    #WOLFSSH_SFTP
    #WOLFSSH_FATFS
)

target_link_libraries(echoServer
    pico_stdlib
    pico_rand
    pico_cyw43_arch_lwip_sys_freertos
    FreeRTOS-Kernel-Heap4
    wolfssh
)

if (USE_UART)
pico_enable_stdio_usb(echoServer 0)
pico_enable_stdio_uart(echoServer 1)
else()
pico_enable_stdio_usb(echoServer 1)
pico_enable_stdio_uart(echoServer 0)
endif()

pico_add_extra_outputs(echoServer)
