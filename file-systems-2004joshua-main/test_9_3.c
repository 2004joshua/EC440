// test_9_3.c
#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define BYTES_1MB 1048576

int main() {
  const char *disk_name = "test_fs";
  char *write_buf = malloc(BYTES_1MB);
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

  // Check file size
  assert(fs_get_filesize(fd) == BYTES_1MB);

  // Clean up
  assert(fs_close(fd) == 0);
  assert(umount_fs(disk_name) == 0);
  assert(remove(disk_name) == 0);
  free(write_buf);

  printf("Test 9.3 passed!\n");
  return 0;
}
