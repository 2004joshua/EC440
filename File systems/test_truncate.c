#include "fs.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main() {
    const char *disk_name = "test_fs";
    const char *file_name = "test_file";
    char write_buf[] = "hello world";
    char truncated[] = "hello";
    char read_buf[sizeof(write_buf)] = {0};

    remove(disk_name); // Remove disk if it exists
    assert(make_fs(disk_name) == 0);
    assert(mount_fs(disk_name) == 0);

    assert(fs_create(file_name) == 0);
    int fd = fs_open(file_name);
    assert(fd >= 0);
    assert(fs_write(fd, write_buf, sizeof(write_buf)) == sizeof(write_buf));
    int file_size = fs_get_filesize(fd);
    assert(file_size == sizeof(write_buf));

    // Truncate should fail with invalid sizes
    assert(fs_truncate(fd, -1) == -1); // Invalid size, passing negative value
    assert(fs_truncate(fd, file_size + 1) == -1); // Trying to extend the file beyond its size, not supported in this context

    // Proper truncation
    off_t new_size = strlen(truncated); // Size after truncation
    assert(fs_truncate(fd, new_size) == 0); // Truncate the file to the size of 'truncated'
    assert(fs_get_filesize(fd) == new_size); // Check if filesize matches new_size after truncation

    // Read back the data after truncation
    assert(fs_lseek(fd, 0) == 0); // Seek back to the start of the file
    assert(fs_read(fd, read_buf, sizeof(read_buf)) == new_size); // Read should match the new truncated size
    assert(strncmp(read_buf, truncated, new_size) == 0); // Data should match 'truncated' string

    // Cleanup
    assert(fs_close(fd) == 0); // Close the file descriptor
    assert(fs_delete(file_name) == 0); // Ensure we can delete the file after closing it

    assert(umount_fs(disk_name) == 0); // Unmount the filesystem

    // Final cleanup and disk removal
    assert(remove(disk_name) == 0); // Ensure the disk file can be removed

    printf("All tests passed successfully.\n");

    return 0;
}

