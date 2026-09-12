add_executable(tls_Client
    ../src/wifi.c
    ../src/tcp.c
    ../src/wolfip_platform.c
    ../src/tls.c
    ../src/time.c
    ../src/idleMemory.c
    ../src/tlsClient_main.c
)

target_compile_definitions(tls_Client PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    TEST_TCP_SERVER_IP=\"${TEST_TCP_SERVER_IP}\"
    CYW43_LWIP=0
    PICO_CYW43_ARCH_FREERTOS=1
    DATETIME=\"${DATETIME}\"
    WOLFSSL_USER_SETTINGS
)

target_link_libraries(tls_Client
    pico_stdlib
    pico_rand
    pico_cyw43_arch_sys_freertos
    FreeRTOS-Kernel-Heap4
    wolfssl
    wolfip
)

if (USE_UART)
    pico_enable_stdio_usb(tls_Client 0)
    pico_enable_stdio_uart(tls_Client 1)
else()
    pico_enable_stdio_usb(tls_Client 1)
    pico_enable_stdio_uart(tls_Client 0)
endif()

pico_add_extra_outputs(tls_Client)
