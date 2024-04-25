// test_9_6.c
#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_1MB 1048576

int main() {
  const char *disk_name = "test_fs";
  char *write_buf = malloc(BYTES_1MB);
  char *read_buf = malloc(BYTES_1MB);
  const char *file_name = "test_file";

  // Fill the write buffer with data
  for (int i = 0; i < BYTES_1MB; i++) {
    write_buf[i] = 'A' + (i % 26);
  }

  // Set up file system
  remove(disk_name);
  assert(make_fs(disk_name) == 0);
  assert(mount_fs(disk_name) == 0);

  // Create and write to file
  assert(fs_create(file_name) == 0);
  int fd = fs_open(file_name);
  assert(fd >= 0);
  assert(fs_write(fd, write_buf, BYTES_1MB) == BYTES_1MB);

  // Overwrite bytes 500-600
  memset(write_buf + 500, 'B', 100);
  assert(fs_lseek(fd, 500) == 0);
  assert(fs_write(fd, write_buf + 500, 100) == 100);

  // Read and verify contents
  assert(fs_lseek(fd, 0) == 0);
  assert(fs_read(fd, read_buf, BYTES_1MB) == BYTES_1MB);
  assert(memcmp(read_buf, write_buf, BYTES_1MB) == 0);

  // Clean up
  assert(fs_close(fd) == 0);
  assert(umount_fs(disk_name) == 0);
  assert(remove(disk_name) == 0);
  free(write_buf);
  free(read_buf);

  printf("Test 9.6 passed!\n");
  return 0;
}
