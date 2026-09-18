#include <string.h>
#include "FreeRTOS.h"
#include "ff.h"
#include "diskio.h"
#include "wolf/ramdisk.h"

static BYTE* ramdisk;

int ramdisk_create(void)
{
    if (ramdisk != NULL)
        return -1;
    ramdisk = pvPortMalloc(RAMDISK_SECTOR_SIZE * RAMDISK_SECTOR_COUNT);
    if (ramdisk == NULL)
        return -1;
    memset(ramdisk, 0, RAMDISK_SECTOR_SIZE * RAMDISK_SECTOR_COUNT);
    return 0;
}

void ramdisk_destroy(void)
{
    vPortFree(ramdisk);
    ramdisk = NULL;
}

DSTATUS disk_status(BYTE drive)
{
    return drive == 0 && ramdisk != NULL ? 0 : STA_NOINIT;
}

DSTATUS disk_initialize(BYTE drive)
{
    return disk_status(drive);
}

DRESULT disk_read(BYTE drive, BYTE* buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || buffer == NULL || count == 0 ||
            sector >= RAMDISK_SECTOR_COUNT ||
            count > RAMDISK_SECTOR_COUNT - sector)
        return RES_PARERR;
    if (ramdisk == NULL)
        return RES_NOTRDY;
    memcpy(buffer, ramdisk + sector * RAMDISK_SECTOR_SIZE,
            count * RAMDISK_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_write(BYTE drive, const BYTE* buffer, LBA_t sector, UINT count)
{
    if (drive != 0 || buffer == NULL || count == 0 ||
            sector >= RAMDISK_SECTOR_COUNT ||
            count > RAMDISK_SECTOR_COUNT - sector)
        return RES_PARERR;
    if (ramdisk == NULL)
        return RES_NOTRDY;
    memcpy(ramdisk + sector * RAMDISK_SECTOR_SIZE, buffer,
            count * RAMDISK_SECTOR_SIZE);
    return RES_OK;
}

DRESULT disk_ioctl(BYTE drive, BYTE command, void* buffer)
{
    if (drive != 0)
        return RES_PARERR;
    if (ramdisk == NULL)
        return RES_NOTRDY;
    if (command == CTRL_SYNC)
        return RES_OK;
    if (buffer == NULL)
        return RES_PARERR;
    switch (command) {
        case GET_SECTOR_COUNT:
            *(LBA_t*)buffer = RAMDISK_SECTOR_COUNT;
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buffer = RAMDISK_SECTOR_SIZE;
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buffer = 1;
            return RES_OK;
        default:
            return RES_PARERR;
    }
}
