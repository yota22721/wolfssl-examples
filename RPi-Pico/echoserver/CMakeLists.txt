add_executable(ssh_EchoServer
    ../src/wifi.c
    ../src/tcp.c
    ../src/wolfip_platform.c
    ../src/wolfssh_bsd_io.c
    ../src/wolfssh_pico_port.c
    ../src/sshServer_main.c
    ../src/idleMemory.c
)

target_compile_definitions(ssh_EchoServer PRIVATE
    CYW43_LWIP=0
    PICO_CYW43_ARCH_FREERTOS=1
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    WOLFSSL_USER_SETTINGS
    WOLFSSH_PICO_BUILD
    WOLFSSH_PICO
    WOLFSSH_TASK_STACK_WORDS=${WOLFSSH_TASK_STACK_WORDS}
)

target_link_libraries(ssh_EchoServer
    pico_stdlib
    pico_rand
    pico_cyw43_arch_sys_freertos
    FreeRTOS-Kernel-Heap4
    wolfssh
    wolfip
)

if (USE_UART)
    pico_enable_stdio_usb(ssh_EchoServer 0)
    pico_enable_stdio_uart(ssh_EchoServer 1)
else()
    pico_enable_stdio_usb(ssh_EchoServer 1)
    pico_enable_stdio_uart(ssh_EchoServer 0)
endif()

pico_add_extra_outputs(ssh_EchoServer)
