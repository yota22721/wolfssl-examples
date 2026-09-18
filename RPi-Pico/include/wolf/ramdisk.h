#ifndef WOLF_RAMDISK_H
#define WOLF_RAMDISK_H
#define RAMDISK_SECTOR_SIZE 512U
#define RAMDISK_SECTOR_COUNT 128U
int ramdisk_create(void);
void ramdisk_destroy(void);
#endif
