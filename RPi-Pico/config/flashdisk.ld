/* Fail the link before the firmware can grow into the persistent filesystem. */
ASSERT(__flash_binary_end <=
       ORIGIN(FLASH) + LENGTH(FLASH) - __wolfssh_flash_disk_size,
       "ssh_EchoServer overlaps the reserved wolfSSH flash disk")
