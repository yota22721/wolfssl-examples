#include <stdint.h>
#include <string.h>

#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"

#include "ff.h"
#include "diskio.h"
#include "wolf/flashdisk.h"

#if WOLFSSH_FLASH_DISK_SIZE == 0
#error WOLFSSH_FLASH_DISK_SIZE must be greater than zero
#endif

#if (WOLFSSH_FLASH_DISK_SIZE % FLASH_SECTOR_SIZE) != 0
#error WOLFSSH_FLASH_DISK_SIZE must be a multiple of FLASH_SECTOR_SIZE
#endif

#if (FLASH_SECTOR_SIZE % FLASHDISK_SECTOR_SIZE) != 0
#error FLASH_SECTOR_SIZE must be a multiple of FLASHDISK_SECTOR_SIZE
#endif

#if WOLFSSH_FLASH_DISK_SIZE > PICO_FLASH_SIZE_BYTES
#error WOLFSSH_FLASH_DISK_SIZE exceeds the board flash size
#endif

#define FLASHDISK_OFFSET (PICO_FLASH_SIZE_BYTES - WOLFSSH_FLASH_DISK_SIZE)

typedef struct FlashMutation {
    uint32_t offset;
    const uint8_t* data;
} FlashMutation;

static uint8_t flashSector[FLASH_SECTOR_SIZE]
    __attribute__((aligned(FLASH_PAGE_SIZE)));
static int flashdiskReady;

static void flashdisk_mutate(void* context)
{
    const FlashMutation* mutation = (const FlashMutation*)context;

    flash_range_erase(mutation->offset, FLASH_SECTOR_SIZE);
    flash_range_program(mutation->offset, mutation->data, FLASH_SECTOR_SIZE);
}

static const uint8_t* flashdisk_address(uint32_t offset)
{
    return (const uint8_t*)(XIP_BASE + FLASHDISK_OFFSET + offset);
}

int flashdisk_create(void)
{
    extern char __flash_binary_end;
    uintptr_t binaryEnd = (uintptr_t)&__flash_binary_end - XIP_BASE;

    if (flashdiskReady || binaryEnd > FLASHDISK_OFFSET)
        return -1;

    flashdiskReady = 1;
    return 0;
}

void flashdisk_destroy(void)
{
    flashdiskReady = 0;
}

DSTATUS disk_status(BYTE drive)
{
    return drive == 0 && flashdiskReady ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE drive)
{
    return disk_status(drive);
}

DRESULT disk_read(BYTE drive, BYTE* buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || buffer == NULL || count == 0 ||
            sector >= FLASHDISK_SECTOR_COUNT ||
            count > FLASHDISK_SECTOR_COUNT - sector)
        return RES_PARERR;
    if (!flashdiskReady)
        return RES_NOTRDY;

    memcpy(buffer, flashdisk_address((uint32_t)sector * FLASHDISK_SECTOR_SIZE),
            (size_t)count * FLASHDISK_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t sector, UINT count)
{
    uint32_t writeOffset;
    size_t remaining;

    if (drive != 0 || buffer == NULL || count == 0 ||
            sector >= FLASHDISK_SECTOR_COUNT ||
            count > FLASHDISK_SECTOR_COUNT - sector)
        return RES_PARERR;
    if (!flashdiskReady)
        return RES_NOTRDY;

    writeOffset = (uint32_t)sector * FLASHDISK_SECTOR_SIZE;
    remaining = (size_t)count * FLASHDISK_SECTOR_SIZE;

    while (remaining != 0) {
        uint32_t eraseOffset = writeOffset & ~(FLASH_SECTOR_SIZE - 1U);
        uint32_t sectorOffset = writeOffset - eraseOffset;
        size_t chunk = FLASH_SECTOR_SIZE - sectorOffset;
        FlashMutation mutation;

        if (chunk > remaining)
            chunk = remaining;

        memcpy(flashSector, flashdisk_address(eraseOffset),
                sizeof(flashSector));
        if (memcmp(flashSector + sectorOffset, buffer, chunk) != 0) {
            memcpy(flashSector + sectorOffset, buffer, chunk);
            mutation.offset = FLASHDISK_OFFSET + eraseOffset;
            mutation.data = flashSector;
            if (flash_safe_execute(flashdisk_mutate, &mutation,
                    UINT32_MAX) != PICO_OK)
                return RES_ERROR;
        }

        buffer += chunk;
        writeOffset += (uint32_t)chunk;
        remaining -= chunk;
    }

    return RES_OK;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void* buffer)
{
    if (drive != 0)
        return RES_PARERR;
    if (!flashdiskReady)
        return RES_NOTRDY;
    if (command == CTRL_SYNC)
        return RES_OK;
    if (buffer == NULL)
        return RES_PARERR;

    switch (command) {
        case GET_SECTOR_COUNT:
            *(LBA_t*)buffer = FLASHDISK_SECTOR_COUNT;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buffer = FLASHDISK_SECTOR_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buffer = FLASH_SECTOR_SIZE / FLASHDISK_SECTOR_SIZE;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
