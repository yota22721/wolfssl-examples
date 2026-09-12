add_executable(tcp_Server
    ../src/wifi.c
    ../src/tcp.c
    ../src/wolfip_platform.c
    ../src/tcpServer_main.c
    ../src/idleMemory.c
)

target_compile_definitions(tcp_Server PRIVATE
    CYW43_LWIP=0
    PICO_CYW43_ARCH_FREERTOS=1
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)

target_link_libraries(tcp_Server
    pico_stdlib
    pico_rand
    pico_cyw43_arch_sys_freertos
    wolfssl
    wolfip
    FreeRTOS-Kernel-Heap4
)

if (USE_UART)
    pico_enable_stdio_usb(tcp_Server 0)
    pico_enable_stdio_uart(tcp_Server 1)
else()
    pico_enable_stdio_usb(tcp_Server 1)
    pico_enable_stdio_uart(tcp_Server 0)
endif()

pico_add_extra_outputs(tcp_Server)
