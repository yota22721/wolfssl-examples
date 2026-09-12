add_executable(tcp_Client
    ../src/wifi.c
    ../src/tcp.c
    ../src/wolfip_platform.c
    ../src/tcpClient_main.c
    ../src/idleMemory.c
)

target_compile_definitions(tcp_Client PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    TEST_TCP_SERVER_IP=\"${TEST_TCP_SERVER_IP}\"
    NO_SYS=0
    CYW43_LWIP=0
    PICO_CYW43_ARCH_FREERTOS=1
    PING_USE_SOCKETS=1
)

target_link_libraries(tcp_Client
    pico_stdlib
    pico_rand
    pico_cyw43_arch_sys_freertos
    wolfssl
    wolfip
    FreeRTOS-Kernel-Heap4
)

if (USE_UART)
pico_enable_stdio_usb(tcp_Client 0)
pico_enable_stdio_uart(tcp_Client 1)
else()
pico_enable_stdio_usb(tcp_Client 1)
pico_enable_stdio_uart(tcp_Client 0)
endif()

pico_add_extra_outputs(tcp_Client)
