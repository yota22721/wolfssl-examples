#ifndef WOLF_FLASHDISK_H
#define WOLF_FLASHDISK_H

#ifndef WOLFSSH_FLASH_DISK_SIZE
#define WOLFSSH_FLASH_DISK_SIZE (64U * 1024U)
#endif

#define FLASHDISK_SECTOR_SIZE 512U
#define FLASHDISK_SECTOR_COUNT \
    (WOLFSSH_FLASH_DISK_SIZE / FLASHDISK_SECTOR_SIZE)

int flashdisk_create(void);
void flashdisk_destroy(void);

#endif
