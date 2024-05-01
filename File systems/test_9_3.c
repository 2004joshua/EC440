#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BYTES_KB 1024
#define BYTES_MB (1024 * BYTES_KB)
#define NUM_FILES 1

int main() {
    const char *disk_name = "test_fs";
    char write_buf0[BYTES_MB];
    const char *file_names[NUM_FILES] = {"file1"};
    int file_index = 0;
    int fds[NUM_FILES];

    memset(write_buf0, 'a', sizeof(write_buf0));

    remove(disk_name); // remove disk if it exists
    assert(make_fs(disk_name) == 0);
    assert(mount_fs(disk_name) == 0);

    // 9.3) Create 1 MiB file, write 1 MiB data --> check size
    assert(fs_create(file_names[file_index]) == 0);
    fds[file_index] = fs_open(file_names[file_index]);
    assert(fds[file_index] >= 0);

    int bytes_written = fs_write(fds[file_index], write_buf0, BYTES_MB);
    printf("Bytes written: %d\n", bytes_written);
    assert(bytes_written == BYTES_MB);

    int file_size = fs_get_filesize(fds[file_index]);
    printf("File size: %d\n", file_size);
    assert(file_size == BYTES_MB);

    assert(fs_close(fds[file_index]) == 0);
    assert(fs_delete(file_names[file_index]) == 0);

    // Clean up
    assert(umount_fs(disk_name) == 0);
    assert(remove(disk_name) == 0);

    printf("Test 9.3 passed!\n");

    return 0;
}

