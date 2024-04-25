#include "fs.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

int main() {
    const char *disk_name = "test_fs";
    remove(disk_name); // Ensure a clean start

    // Attempting to mount a non-existent disk should fail
    assert(mount_fs(disk_name) == -1);

    // Create the filesystem, which should succeed
    assert(make_fs(disk_name) == 0);

    // Attempting to unmount a disk that hasn't been mounted should fail
    assert(umount_fs(disk_name) == -1);

    // Mount and then unmount the newly created filesystem, both should succeed
    assert(mount_fs(disk_name) == 0);
    assert(umount_fs(disk_name) == 0);

    // Open the filesystem disk file to inspect its content
    FILE *fp = fopen(disk_name, "rb"); // Open in binary mode to read all bytes as-is
    assert(fp != NULL);

    // Check that some metadata is written to the disk (i.e., not all bytes are zero)
    bool non_zero_byte_found = false;
    char byte;
    while (fread(&byte, 1, 1, fp)) { // Read byte by byte
        if (byte != 0) {
            non_zero_byte_found = true;
            break;
        }
    }
    assert(non_zero_byte_found); // Ensure at least one byte in the disk file is non-zero
    assert(fclose(fp) == 0); // Close the file pointer

    // Clean up by removing the disk file
    assert(remove(disk_name) == 0);

    return 0;
}

