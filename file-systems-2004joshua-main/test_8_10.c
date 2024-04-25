// test_8_10.c
#include "fs.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define BYTES_1K 1024

int main() {
  const char *disk_name = "test_fs";
  char write_buf[BYTES_1K];
  char read_buf[BYTES_1K];
  const char *file_name = "test_file";

  // Fill the write buffer with data
  for (int i = 0; i < BYTES_1K; i++) {
    write_buf[i] = 'A' + (i % 26);
  }

  // Set up file system and write to file
  remove(disk_name);
  assert(make_fs(disk_name) == 0);
  assert(mount_fs(disk_name) == 0);
  assert(fs_create(file_name) == 0);
  int fd = fs_open(file_name);
  assert(fd >= 0);
  assert(fs_write(fd, write_buf, BYTES_1K) == BYTES_1K);
  assert(fs_close(fd) == 0);
  assert(umount_fs(disk_name) == 0);

  // Simulate a different process by remounting the file system
  assert(mount_fs(disk_name) == 0);

  // Open and read from file
  fd = fs_open(file_name);
  assert(fd >= 0);
  assert(fs_read(fd, read_buf, BYTES_1K) == BYTES_1K);

  // Verify contents
  assert(memcmp(write_buf, read_buf, BYTES_1K) == 0);

  // Clean up
  assert(fs_close(fd) == 0);
  assert(umount_fs(disk_name) == 0);
  assert(remove(disk_name) == 0);

  printf("Test 8.10 passed!\n");
  return 0;
}
