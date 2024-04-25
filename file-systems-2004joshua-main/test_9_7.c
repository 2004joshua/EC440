// test_9_7.c
#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define BYTES_1MB 1048576
#define NUM_FILES 16

int main() {
  const char *disk_name = "test_fs";
  char *write_buf = malloc(BYTES_1MB);
  char *read_buf = malloc(BYTES_1MB);
  char file_name[16];

  // Fill the write buffer with data
  for (int i = 0; i < BYTES_1MB; i++) {
    write_buf[i] = 'A' + (i % 26);
  }

  // Set up file system
  remove(disk_name);
  assert(make_fs(disk_name) == 0);
  assert(mount_fs(disk_name) == 0);

  // Create, write, and verify 16 files
  for (int i = 0; i < NUM_FILES; i++) {
    sprintf(file_name, "file%d", i);
    assert(fs_create(file_name) == 0);
    int fd = fs_open(file_name);
    assert(fd >= 0);
    assert(fs_write(fd, write_buf, BYTES_1MB) == BYTES_1MB);
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, read_buf, BYTES_1MB) == BYTES_1MB);
    assert(memcmp(read_buf, write_buf, BYTES_1MB) == 0);
    assert(fs_close(fd) == 0);
  }

  // Clean up
  assert(umount_fs(disk_name) == 0);
  assert(remove(disk_name) == 0);
  free(write_buf);
  free(read_buf);

  printf("Test 9.7 passed!\n");
  return 0;
}
