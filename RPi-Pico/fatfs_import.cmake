set(FATFS_SOURCE_DIR ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source)
set(FATFS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/fatfs)
foreach(FATFS_FILE ff.c ff.h diskio.h)
    configure_file(${FATFS_SOURCE_DIR}/${FATFS_FILE}
        ${FATFS_BUILD_DIR}/${FATFS_FILE} COPYONLY)
endforeach()
configure_file(config/ffconf.h ${FATFS_BUILD_DIR}/ffconf.h COPYONLY)

target_compile_definitions(FreeRTOS-Kernel INTERFACE "configTOTAL_HEAP_SIZE=(256*1024)")
set(WOLFSSH_FATFS_STORAGE "RAM" CACHE STRING
    "FatFs backing store for the wolfSSH echoserver (RAM or FLASH)")
set_property(CACHE WOLFSSH_FATFS_STORAGE PROPERTY STRINGS RAM FLASH)
string(TOUPPER "${WOLFSSH_FATFS_STORAGE}" WOLFSSH_FATFS_STORAGE)

if (WOLFSSH_FATFS_STORAGE STREQUAL "RAM")
    set(FATFS_DISK_SOURCE src/ramdisk.c)
elseif (WOLFSSH_FATFS_STORAGE STREQUAL "FLASH")
    math(EXPR WOLFSSH_FLASH_DISK_SIZE "64 * 1024")
    set(FATFS_DISK_SOURCE src/flashdisk.c)
else()
    message(FATAL_ERROR "WOLFSSH_FATFS_STORAGE must be RAM or FLASH")
endif()

add_library(fatfs STATIC ${FATFS_BUILD_DIR}/ff.c ${FATFS_DISK_SOURCE})
target_include_directories(fatfs PUBLIC ${FATFS_BUILD_DIR})
target_link_libraries(fatfs PUBLIC FreeRTOS-Kernel-Heap4)

if (WOLFSSH_FATFS_STORAGE STREQUAL "FLASH")
    target_compile_definitions(fatfs PUBLIC
        WOLFSSH_FLASH_DISK
        WOLFSSH_FLASH_DISK_SIZE=${WOLFSSH_FLASH_DISK_SIZE}
    )
    target_link_libraries(fatfs PUBLIC pico_flash hardware_flash)
endif()
