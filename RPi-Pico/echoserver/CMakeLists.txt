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
)

target_link_libraries(echoServer
    pico_stdlib
    pico_rand
    pico_cyw43_arch_lwip_sys_freertos
    FreeRTOS-Kernel-Heap4
    wolfssh
)

pico_enable_stdio_usb(echoServer 1)
pico_enable_stdio_uart(echoServer 1)

pico_add_extra_outputs(echoServer)