add_executable(tls_Server
    ../src/wifi.c
    ../src/tcp.c
    ../src/wolfip_platform.c
    ../src/tls.c
    ../src/time.c
    ../src/idleMemory.c
    ../src/tlsServer_main.c
)

target_compile_definitions(tls_Server PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    CYW43_LWIP=0
    PICO_CYW43_ARCH_FREERTOS=1
    DATETIME=\"${DATETIME}\"
    WOLFSSL_USER_SETTINGS
)

target_link_libraries(tls_Server
    pico_stdlib
    pico_rand
    pico_cyw43_arch_sys_freertos
    FreeRTOS-Kernel-Heap4
    wolfssl
    wolfip
)

if (USE_UART)
    pico_enable_stdio_usb(tls_Server 0)
    pico_enable_stdio_uart(tls_Server 1)
else()
    pico_enable_stdio_usb(tls_Server 1)
    pico_enable_stdio_uart(tls_Server 0)
endif()

pico_add_extra_outputs(tls_Server)
