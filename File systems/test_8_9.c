#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_8K 8000

int main() {
    const char *disk_name = "test_fs";
    char write_buf[BYTES_8K];
    char read_buf[BYTES_8K];
    const char *file_name = "test_file";

    // Fill the write buffer with data
    for (int i = 0; i < BYTES_8K; i++) {
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
    assert(fs_write(fd, write_buf, BYTES_8K) == BYTES_8K);

    // Read from file
    assert(fs_lseek(fd, 0) == 0);
    assert(fs_read(fd, read_buf, BYTES_8K) == BYTES_8K);

    // Verify contents
    assert(memcmp(write_buf, read_buf, BYTES_8K) == 0);

    // Clean up
    assert(fs_close(fd) == 0);
    assert(umount_fs(disk_name) == 0);
    assert(remove(disk_name) == 0);

    printf("Test 8.9 passed!\n");
    return 0;
}
