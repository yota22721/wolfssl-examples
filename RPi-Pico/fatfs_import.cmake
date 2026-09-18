set(FATFS_SOURCE_DIR ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source)
set(FATFS_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/fatfs)
foreach(FATFS_FILE ff.c ff.h diskio.h)
    configure_file(${FATFS_SOURCE_DIR}/${FATFS_FILE}
        ${FATFS_BUILD_DIR}/${FATFS_FILE} COPYONLY)
endforeach()
configure_file(config/ffconf.h ${FATFS_BUILD_DIR}/ffconf.h COPYONLY)

target_compile_definitions(FreeRTOS-Kernel INTERFACE "configTOTAL_HEAP_SIZE=(256*1024)")
add_library(fatfs STATIC ${FATFS_BUILD_DIR}/ff.c src/ramdisk.c)
target_include_directories(fatfs PUBLIC ${FATFS_BUILD_DIR})
target_link_libraries(fatfs PUBLIC FreeRTOS-Kernel-Heap4)
