
#include <stdint.h> 
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>

//custom headers
#include "fs.h"
#include "disk.h" 

//definitions
#define MAX_FNAME_SIZE 16
#define MAX_BLOCK_SIZE 4096                   // 4KB
#define MAX_FILDES 32                         // max file descriptors
#define MAX_FILES 64
#define DISK_BLOCKS 8192
#define BITMAP_SIZE (DISK_BLOCKS / 8)

//file types
enum ftype{
  REGULAR,
  DIRECTORY,
  NOTHING
};

/* 
Super Block: typically first block of the disk and stores info on other data structures 
  * For each persistent data structure, specify where it is on the disk
*/ 

struct superblock {
  // how many blocks are being used
  uint16_t ub_bitmap_count;
  // where the list of blank and writted blocks start
  uint16_t ub_bitmap_offset; 
  // how many blocks are used to keep a record of each file
  uint16_t im_blocks; 
  // number where the record of files start
  uint16_t im_offset;
  // set a flag to represent if superblock was modified in any way
  uint8_t dirty; 
};

/* 
Directory: holds the files, in this case itll hold the mapping from names to inodes 
  * Map file names to inode numbers
  * Only need the Root node
*/
struct dentry{
  // dir name
  char name[MAX_FNAME_SIZE]; 
  // is it being used? 
  uint8_t is_used; 
  // inode number
  uint16_t inode_num; 
};

/* 
Inode: Metadata for files 
  * Map a file to the locations of its potentially non-contiguous data on disk
*/
struct inode{
  // file type
  enum ftype type; 
  // file size in bytes
  uint32_t size; 
  // points directly to data blocks
  uint16_t direct_offset[10]; 
  // points to a list of direct block addresses
  uint16_t single_indirect;
  // points to a block containing a list of single indirect block addresses
  uint16_t double_indirect;
  // is it in use?
  uint8_t is_used;
  //index in its array
  uint8_t inode_num;
  // dirty? 
  uint8_t dirty; 
};

/* 
File Descriptor: represents opening a file, tracking the inode number, and current offset 
  * Map an open file to an inode
  * Track position in the file through read/write/truncate
*/
struct FD{
  // is it being used? 
  uint8_t is_used; 
  // inode number
  uint16_t inode_num;
  // position within file
  uint16_t offset; 
}; 

struct bitmap_info{
  uint8_t ub_bitmap[DISK_BLOCKS / CHAR_BIT];
  uint8_t dirty; 
};

struct superblock fs; 
struct bitmap_info ubm; 
struct dentry root[MAX_FILES]; 
struct FD fds[MAX_FILDES]; 
struct inode inodes[MAX_FILES]; 

bool mounted = false; 

/*
  BITMAP hints: 
    * 1 bit per block indicating if it's free or not
    * All of the bits get combined into one long series of bits and written to a location on disk 
    * biggest data tpye we have is 64 bits, but we need ~8192
    * Solution: use an array: used_block_bitmap[DISK_BLOCKS / CHAR_BIT]
    * Write helper functions to perform single bit operations 
      * get, set, clear
      * (/), (%), (&) will help!  
*/

void initialize_fs_structs() {
    // Initialize the superblock
    fs.ub_bitmap_count = 0;
    fs.ub_bitmap_offset = 0;
    fs.im_blocks = 0;
    fs.im_offset = 0;
    fs.dirty = false;

    memset(ubm.ub_bitmap, 0, sizeof(ubm.ub_bitmap));
    
    ubm.dirty = false;

    for (int i = 0; i < MAX_FILES; i++) {
        memset(root[i].name, 0, MAX_FNAME_SIZE); 
        root[i].is_used = false;
        root[i].inode_num = i; 
    }

    for (int i = 0; i < MAX_FILES; i++) {
        inodes[i].type = NOTHING; 
        inodes[i].size = 0;
        memset(inodes[i].direct_offset, 0, sizeof(inodes[i].direct_offset));
        inodes[i].single_indirect = 0;
        inodes[i].double_indirect = 0;
        inodes[i].is_used = false;
        inodes[i].inode_num = i; 
        inodes[i].dirty = false;
    }

    for (int i = 0; i < MAX_FILDES; i++) {
        fds[i].is_used = false;
        fds[i].inode_num = 0; 
        fds[i].offset = 0; 
    }
}


void set_bit(int block_num){
  uint16_t index = block_num / 8; 
  uint16_t pos = block_num % 8; 
  ubm.ub_bitmap[index] |= (1  << pos); 
  ubm.dirty = true; 
}

int get_bit(int block_num){
  uint16_t index = block_num / 8; 
  uint16_t pos = block_num % 8; 
  return (ubm.ub_bitmap[index] & (1  << pos)) != 0; 
}

void clear_bit(int block_num){
  uint16_t index = block_num / 8; 
  uint16_t pos = block_num % 8; 
  ubm.ub_bitmap[index] &= ~(1  << pos); 
  fs.ub_bitmap_count--; 
}

int get_free_block(){
  for(int i = 0; i < DISK_BLOCKS; i++){
    if(get_bit(i) == 0){
      // a free block was found
      set_bit(i); 
      fs.ub_bitmap_count++; 
      return i; 
    }
  }

  // no free block was found!
  return -1; 
}

// Management Routines

int make_fs(const char* disk_name){
  
  /*
  This function creates a fresh (and empty) file system on the virtual disk with name disk_name.
  As part of this function, you should first invoke make_disk(disk_name) to create a new disk.
  Then, open this disk and write/initialize the necessary meta-information for your file system so
  that it can be later used (mounted). The function returns 0 on success, and -1 if the disk
  disk_name could not be created, opened, or properly initialized.
  */
  
  if(make_disk(disk_name) != 0){
    fprintf(stderr, "ERROR: Failure to create disk!\n"); 
    return -1; 
  }

  if(open_disk(disk_name) != 0){
    fprintf(stderr, "ERROR: Failure to open disk!\n"); 
    return -1;
  }

  initialize_fs_structs(); 

  // blocks needed for bitmap
  fs.ub_bitmap_count = (DISK_BLOCKS / 8 + MAX_BLOCK_SIZE - 1) / MAX_BLOCK_SIZE;
  // bitmap starts right after superblock
  fs.ub_bitmap_offset = 1;
  // total size for all inodes in bytes and determine blocks needed 
  size_t total_inode_size = sizeof(struct inode) * MAX_FILES; 
  fs.im_blocks = (total_inode_size + MAX_BLOCK_SIZE - 1) / MAX_BLOCK_SIZE; 
  // inode metadata start offset
  fs.im_offset = fs.ub_bitmap_count + fs.ub_bitmap_offset;  

  // store the data needed for the disk
  char buffer[MAX_BLOCK_SIZE]; 
  memset(buffer, 0, MAX_BLOCK_SIZE); 
  memcpy(buffer, &fs, sizeof(fs)); 
  if(block_write(0, buffer) != 0){
    fprintf(stderr, "ERROR: Failure to write!\n"); 
    close_disk(); 
    return -1; 
  }
  
  memset(buffer, 0, MAX_BLOCK_SIZE); 
  for(int i = 0; i < fs.im_offset + fs.im_blocks; i++){
    set_bit(i); 
  }
  
  if(block_write(fs.ub_bitmap_offset, ubm.ub_bitmap) != 0){
    fprintf(stderr, "ERROR: Failure to write bitmap!\n"); 
    close_disk(); 
    return -1; 
  }

  memset(buffer, 0, MAX_BLOCK_SIZE); 
  for(int i = 0; i < fs.im_blocks; i++){
    if(block_write(fs.im_offset + i, buffer) != 0){
      fprintf(stderr, "ERROR: Failure to write in inode table");
      close_disk(); 
      return -1; 
    }
  }

  if(close_disk() != 0){
    fprintf(stderr, "ERROR: Failure to close disk!\n"); 
    return -1; 
  }

  return 0;
}

int mount_fs(const char *disk_name){

  /*
  This function mounts a file system that is stored on a virtual disk with name disk_name. With
  the mount operation, a file system becomes "ready for use." You need to open the disk and
  then load the meta-information that is necessary to handle the file system operations that are
  discussed below. The function returns 0 on success, and -1 when the disk disk_name could not
  be opened or when the disk does not contain a valid file system (that you previously created
  with make_fs).
  */

  if(open_disk(disk_name) != 0){
    fprintf(stderr, "ERROR: Failure to open disk!\n"); 
    return -1; 
  }

  char buffer[MAX_BLOCK_SIZE];
  if(block_read(0, buffer) != 0){
    fprintf(stderr, "ERROR: Failure to read super block!\n");
    close_disk();
    return -1;
  }
  memcpy(&fs, buffer, sizeof(fs)); 

  for(int i = 0; i < fs.ub_bitmap_count; i++){
    if(block_read(fs.ub_bitmap_offset + i, buffer) != 0){
      fprintf(stderr, "ERROR: Failure to read bitmap block!\n"); 
      close_disk(); 
      return -1; 
    }  
    memcpy(ubm.ub_bitmap + i * MAX_BLOCK_SIZE, buffer, MAX_BLOCK_SIZE); 
  }

  for(int i = 0; i < fs.im_blocks; i++){
    if(block_read(fs.im_offset + i, buffer) != 0){
      fprintf(stderr, "ERROR: Failed to read inode table!\n"); 
      close_disk(); 
      return -1; 
    }

    size_t cpy_size = (i == fs.im_blocks - 1) ? 
    (MAX_FILES % (MAX_BLOCK_SIZE / sizeof(struct inode))) * sizeof(struct inode) : MAX_BLOCK_SIZE;

    memcpy(inodes + i * (MAX_BLOCK_SIZE / sizeof(struct inode)), buffer, cpy_size); 
  }

  mounted = true; 
  return 0; 
}

int umount_fs(const char *disk_name) {
  
  /*
  This function unmounts your file system from a virtual disk with name disk_name. As part of
  this operation, you need to write back all meta-information so that the disk persistently reflects
  all changes that were made to the file system (such as new files that are created, data that is
  written, ...). You should also close the disk. The function returns 0 on success, and -1 when the
  disk disk_name could not be closed or when data could not be written to the disk (this should
  not happen).

  It is important to observe that your file system must provide persistent storage. That is, assume
  that you have created a file system on a virtual disk and mounted it. Then, you create a few
  files and write some data to them. Finally, you unmount the file system. At this point, all data
  must be written onto the virtual disk. Another program that mounts the file system at a later
  point in time must see the previously created files and the data that was written. This means
  that whenever umount_fs is called, all meta-information and file data (that you could
  temporarily have only in memory; depending on your implementation) must be written out to
  disk.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n");
    return -1; 
  }

  char buffer[MAX_BLOCK_SIZE]; 

  /* I will write back if any structure is considered dirty */
  
  // super block
  if(fs.dirty){
    memset(buffer, 0, MAX_BLOCK_SIZE); 
    memcpy(buffer, &fs, sizeof(fs)); 

    if(block_write(0, buffer) != 0){
      fprintf(stderr, "ERROR: Failure to write back the superblock!\n");
      return -1;
    }
    fs.dirty = false; 
  }

  // bit map
  if(ubm.dirty){
    struct superblock* super_block = &fs; 
    for(int i = 0; i < super_block->ub_bitmap_count; i++){
      memset(buffer, 0, MAX_BLOCK_SIZE);
      memcpy(buffer, ubm.ub_bitmap + i * MAX_BLOCK_SIZE, MAX_BLOCK_SIZE); 

      if(block_write(super_block->ub_bitmap_offset + i , buffer) != 0){
        fprintf(stderr, "ERROR: Failure to write back the bitmap segment!\n"); 
        return -1; 
      }
    }
    ubm.dirty = false; 
  }

  // inodes
  for(int i = 0; i < MAX_FILES; i++){
    if(inodes[i].dirty){
      memset(buffer, 0, MAX_BLOCK_SIZE);
      memcpy(buffer ,&inodes[i], sizeof(struct inode)); 

      int blknum = fs.im_offset + i / (MAX_BLOCK_SIZE / sizeof(struct inode)); 

      if(block_write(blknum, buffer) != 0){
        fprintf(stderr, "ERROR: Failure to write back the inode number!\n"); 
        return -1; 
      }

      inodes[i].dirty = false; 
    }
  }

  if(close_disk() != 0){
    fprintf(stderr, "ERROR: Disk wouldnt close properly!\n"); 
    return -1; 
  }

  mounted = false;
  return 0; 
}


// File System Functions

int fs_open(const char *name){

  /*
  The file specified by name is opened for reading and writing, and the number of the file
  descriptor corresponding to this file is returned to the calling function. If successful, fs_open
  returns a non-negative integer, which is a file descriptor that can be used to subsequently
  access this file. Note that the same file (file with the same name) can be opened multiple times.
  When this happens, your file system is supposed to provide multiple, independent file
  descriptors. Your library must support a maximum of 32 file descriptors that can be open
  simultaneously.

  fs_open returns -1 on failure. It is a failure when the file with name cannot be found (i.e., it
  has not been created previously or is already deleted). It is also a failure when there are
  already 32 file descriptors active. When a file is opened, the file offset (seek pointer) is set to 0
  (the beginning of the file).
  */
  
  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n"); 
    return -1; 
  } 

  /* Search for the file, if not found id will stay -1 */
  int id = -1; 
  for(int i = 0; i < MAX_FILES; i++){
    if(strcmp(root[i].name, name) == 0 && root[i].is_used){
      id = root[i].inode_num;
      break;
    }
  }

  if(id == -1){
    fprintf(stderr, "ERROR: File not found!\n");
    return -1; 
  }

  int fd_idx = -1; 
  for(int i = 0; i < MAX_FILDES; i++){
    if(!fds[i].is_used){
      fd_idx = i; 
      fds[i].inode_num = id; 
      fds[i].is_used = true; 
      fds[i].offset = 0; 
      return fd_idx;
    }
  }

  fprintf(stderr, "ERROR: No file descriptor is available!\n");
  return -1; 

}

int fs_close(const int fd){
  /*
  The file descriptor fd is closed. A closed file descriptor can no longer be used to access the
  corresponding file. Upon successful completion, a value of 0 is returned. In case the file
  descriptor fd does not exist or is not open, the function returns -1. 
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n"); 
    return -1; 
  }

  if(fd >= MAX_FILDES || fd < 0 || !fds[fd].is_used){
    fprintf(stderr, "ERROR: Invalid file descriptor!\n"); 
    return -1; 
  }

  fds[fd].is_used = false; 
  fds[fd].inode_num = 0; 
  fds[fd].offset = 0; 

  return 0;
}
int fs_create(const char *name) {

  /*
  This function creates a new file with name name in the root directory of your file system. The
  file is initially empty. The maximum length for a file name is 15 characters. Upon successful
  completion, a value of 0 is returned. fs_create returns -1 on failure. It is a failure when the file
  with name already exists, when the file name is too long (it exceeds 15 characters), or when
  the root directory is full (you must support at least 64 files, but you may support more). Note
  that to access a file that is created, it has to be subsequently opened.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n");
    return -1; 
  }

  // check fname size is appropriate
  if(strlen(name) >= MAX_FNAME_SIZE){
    fprintf(stderr, "ERROR: Name is too long!\n");
    return -1; 
  }

  // check if fname exists
  for(int i = 0; i < MAX_FILES; i++){
    if(root[i].is_used && strcmp(name, root[i].name) == 0){
      fprintf(stderr, "ERROR: This file name already exists!\n");
      return -1; 
    }
  }

  // find an unused inode to initialize
  int inode_idx = -1; 
  for(int i = 0; i < MAX_FILES; i++){
    if(!inodes[i].is_used){
      inode_idx = i; 
      inodes[i].is_used = true; 
      inodes[i].size = 0; 
      inodes[i].type = REGULAR; 

      memset(inodes[i].direct_offset, 0, sizeof(inodes[i].direct_offset));
      inodes[i].single_indirect = 0; 
      inodes[i].double_indirect = 0; 

      inodes[i].dirty = true; 
      break; 
    }
  }

  if(inode_idx == -1){
    fprintf(stderr, "ERROR: No available inodes!\n");
    return -1;
  }

  // unused entry in the root directory
  for(int i = 0; i < MAX_FILES; i++){
    if(!root[i].is_used){
      root[i].is_used = true; 
      strncpy(root[i].name, name, MAX_FNAME_SIZE - 1); 
      root[i].name[MAX_FNAME_SIZE - 1] = '\0'; 
      root[i].inode_num = inode_idx; 
      return 0; 
    }
  }

  fprintf(stderr, "ERROR: Root is full!\n"); 
  return -1; 
}


int fs_delete(const char *name){

  /*
  This function deletes the file with name name from the root directory of your file system and
  frees all data blocks and meta-information that correspond to that file. The file that is being
  deleted must not be open. That is, there cannot be any open file descriptor that refers to the file
  name. When the file is open at the time that fs_delete is called, the call fails and the file is not
  deleted. Upon successful completion, a value of 0 is returned. fs_delete returns -1 on failure. It
  is a failure when the file with name does not exist. It is also a failure when the file is currently
  open (i.e., there exists at least one open file descriptor that is associated with this file).
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't open!\n");
    return -1; 
  }

  // check if it even exists in directory
  int file_idx = -1; 
  for(int i = 0; i < MAX_FILES; i++){
    if(strcmp(root[i].name, name) == 0 && root[i].is_used){
      file_idx = i; 
      break; 
    }
  }

  if(file_idx == -1){
    fprintf(stderr, "ERROR: File does not exist!\n");
    return -1; 
  }

  // can't delete an open file
  int inode_num = root[file_idx].inode_num;
  for(int i = 0; i < MAX_FILDES; i++){
    if(fds[i].inode_num == inode_num && fds[i].is_used){
      fprintf(stderr, "ERROR: File is currently open!\n");
      return -1; 
    }
  }

  for(int i = 0; i < 10; i++){
    if(inodes[inode_num].direct_offset[i] != 0){
      clear_bit(inodes[inode_num].direct_offset[i]);
    }
  }

  size_t blkptr = MAX_BLOCK_SIZE / sizeof(uint16_t); 

  // clearing single indirects if there is any
  if(inodes[inode_num].single_indirect != 0){
    uint16_t ib[blkptr]; 
    block_read(inodes[inode_num].single_indirect, ib); 

    for(int i = 0; i < blkptr; i++){
      if(ib[i] != 0){
        clear_bit(ib[i]); 
      }
    }
    clear_bit(inodes[inode_num].single_indirect);
    inodes[inode_num].single_indirect = 0; 
  }

  // clearing double indirects if any
  if(inodes[inode_num].double_indirect != 0){
    uint16_t double_ib[blkptr]; 
    block_read(inodes[inode_num].double_indirect, double_ib); 
    
    for(int i = 0; i < blkptr; i++){
      if(double_ib[i] != 0){
        uint16_t single_ib[blkptr]; 
        block_read(double_ib[i], single_ib); 

        for(int j = 0; j < blkptr; j++){
          if(single_ib[j] != 0){
            clear_bit(single_ib[j]); 
          }
        }
        clear_bit(double_ib[i]); 
      }
    }
    clear_bit(inodes[inode_num].double_indirect);
    inodes[inode_num].double_indirect = 0;
  }

  ubm.dirty = true; 

  inodes[inode_num].is_used = false;
  memset(&inodes[inode_num], 0, sizeof(struct inode)); 

  root[file_idx].is_used = false; 
  memset(&root[file_idx], 0, sizeof(struct dentry)); 

  return 0; 
}

int fs_read(int fildes, void *buf, size_t nbyte){

  /*
  This function attempts to read nbyte bytes of data from the file referenced by the descriptor fd
  into the buffer pointed to by buf. The function assumes that the buffer buf is large enough to
  hold at least nbyte bytes. When the function attempts to read past the end of the file, it reads all
  bytes until the end of the file. Upon successful completion, the number of bytes that were
  actually read is returned. This number could be smaller than nbyte when attempting to read
  past the end of the file (when trying to read while the file pointer is at the end of the file, the
  function returns zero). In case of failure, the function returns -1. It is a failure when the file
  descriptor fd is not valid. The read function implicitly increments the file pointer by the number
  of bytes that were actually read.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n"); 
    return -1; 
  }

  if(fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].is_used){
    fprintf(stderr, "ERROR: Invalid file descriptor!\n"); 
    return -1; 
  }

  struct FD* fd = &fds[fildes]; 
  struct inode* inode = &inodes[fd->inode_num]; 

  if(!inode->is_used){
    fprintf(stderr, "ERROR: Inode isn't in use!\n"); 
    return -1; 
  }

  if(fd->offset >= inode->size){
    // offset is at the eof or surpasses it
    return 0; 
  }

  size_t bytes_read = 0; 

  char* buffer = (char*) buf; 

  size_t block_ptr = MAX_BLOCK_SIZE / sizeof(uint16_t); 

  while (nbyte > 0 && fd->offset < inode->size) {
    size_t block_idx = fd->offset / MAX_BLOCK_SIZE;
    size_t block_off = fd->offset % MAX_BLOCK_SIZE;
    size_t bytes_in_block = MAX_BLOCK_SIZE - block_off;
    size_t read_size = (bytes_in_block < nbyte) ? bytes_in_block : nbyte;

    uint16_t block;

    // Direct blocks
    if (block_idx < 10) {
      block = inode->direct_offset[block_idx];
    }
    // Single indirect
    else if (block_idx < 10 + block_ptr) {
      uint16_t single_ib[block_ptr];
      memset(single_ib, 0, sizeof(single_ib));
      block_read(inode->single_indirect, single_ib);
      block_idx -= 10;
      block = single_ib[block_idx];
    }
    // Double indirect
    else {
      block_idx -= 10 + block_ptr;
      uint16_t double_ib_idx = block_idx / block_ptr;
      uint16_t single_ib[block_ptr];
      memset(single_ib,0,sizeof(single_ib));
      block_read(inode->double_indirect, single_ib);
      block_read(single_ib[double_ib_idx], single_ib);
      block_idx %= block_ptr;
      block = single_ib[block_idx];
    }

    char data[MAX_BLOCK_SIZE];
    block_read(block, data);

    // Adjust read_size if it exceeds file size
    if (fd->offset + read_size > inode->size) {
      read_size = inode->size - fd->offset;
    }

    memcpy(buffer + bytes_read, data + block_off, read_size);
    bytes_read += read_size;
    fd->offset += read_size;
    nbyte -= read_size;
  }

  return bytes_read;
}

int fs_write(int fildes, void *buf, size_t nbyte){

  /*
  This function attempts to write nbyte bytes of data to the file referenced by the descriptor fd
  from the buffer pointed to by buf. The function assumes that the buffer buf holds at least nbyte
  bytes. When the function attempts to write past the end of the file, the file is automatically
  extended to hold the additional bytes. It is possible that the disk runs out of space while
  performing a write operation. In this case, the function attempts to write as many bytes as
  possible (i.e., to fill up the entire space that is left). A file size of at least 1 MiB must be
  supported. Extra credit will be awarded for supporting file sizes of up to 30 MiB and up to 40
  MiB.

  Upon successful completion, the number of bytes that were actually written is returned. This
  number could be smaller than nbyte when the disk runs out of space (when writing to a full
  disk, the function returns zero). In case of failure, the function returns -1. It is a failure when the
  file descriptor fd is not valid. The write function implicitly increments the file pointer by the
  number of bytes that were actually written.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n");
    return -1; 
  }

  if(fildes >= MAX_FILDES || fildes < 0 || !fds[fildes].is_used){
    fprintf(stderr, "ERROR: Invalid file descriptor!\n");
    return -1;
  }

  struct FD* fd = &fds[fildes];
  struct inode* inode = &inodes[fd->inode_num]; 
  if(!inode->is_used){
    fprintf(stderr, "ERROR: Inode isn't in use!\n");
    return -1; 
  }

  const char* buffer = (const char*)buf; 
  const size_t original_offset = fd->offset; 
  size_t bytes_written = 0; 
  size_t block_ptr = MAX_BLOCK_SIZE / sizeof(uint16_t); 

  while(bytes_written < nbyte){
    size_t fpos = fd->offset + bytes_written; 
    size_t block_idx = fpos / MAX_BLOCK_SIZE; 
    size_t block_off = fpos % MAX_BLOCK_SIZE; 
    // bytes remaining within the block
    size_t byrem = MAX_BLOCK_SIZE - block_off;
    size_t byte_write = (byrem < (nbyte - bytes_written)) ? byrem : (nbyte - bytes_written); 

    uint16_t block; 

    // direct blocks
    if(block_idx < 10){
      if(inode->direct_offset[block_idx] == 0){
        int new_block = get_free_block(); 
        if(new_block == -1){
          fprintf(stderr, "ERROR: No free blocks are available!\n"); 
          break; 
        }
        inode->direct_offset[block_idx] = new_block; 
        ubm.dirty = true; 
      }
      block = inode->direct_offset[block_idx]; 
    }

    // single indirect blocks
    else if(block_idx < 10 + block_ptr){
      if(inode->single_indirect == 0){
        int new_block = get_free_block(); 
        if(new_block == -1){
          fprintf(stderr, "ERROR: No free blocks are available!\n"); 
          return bytes_written; 
        } 
        inode->single_indirect = new_block; 
        ubm.dirty = true; 
      }

      uint16_t ib[block_ptr]; 
      if(block_read(inode->single_indirect, &ib) != 0){
        fprintf(stderr, "ERROR: Failed to read single indirect block!\n");
        return -1; 
      }

      if(ib[block_idx - 10] == 0){
        int new_block = get_free_block(); 
        if(new_block == -1){
          fprintf(stderr, "ERROR: No free blocks are available!\n"); 
          return bytes_written; 
        }

        ib[block_idx - 10] = new_block; 
        ubm.dirty = true; 
        if(block_write(inode->single_indirect, &ib) != 0){
          fprintf(stderr, "ERROR: Failed to write single indirect block!\n"); 
          return -1; 
        } 
      }
      block = ib[block_idx - 10]; 
    }

    // double indirect block
    else{
      size_t double_iidx = (block_idx - 10 - block_ptr) / block_ptr; 
      size_t single_iidx = (block_idx - 10 - block_ptr) % block_ptr; 

      if(inode->double_indirect == 0){
        int new_block = get_free_block(); 
        if(new_block == -1){
          fprintf(stderr, "ERROR: No free blocks are available!\n"); 
          return bytes_written; 
        }
        inode->double_indirect = new_block; 
        ubm.dirty = true; 
      }

      uint16_t double_ib[block_ptr]; 
      if(block_read(inode->double_indirect, &double_ib) != 0){
        fprintf(stderr, "ERROR: Failed to read double indirect block!\n"); 
        break; 
      }

      if(double_ib[double_iidx] == 0){
        int new_block = get_free_block(); 
        if(new_block == -1){
          fprintf(stderr, "ERROR: No free blocks are available!\n"); 
          return bytes_written; 
        }

        double_ib[double_iidx] = new_block; 
        ubm.dirty = true; 
        if(block_write(inode->double_indirect, &double_ib) != 0){
          fprintf(stderr, "ERROR: Failed to write double indirect block!\n"); 
          break; 
        }

        uint16_t single_ib[block_ptr]; 
        if(block_read(double_ib[double_iidx], &single_ib) != 0){
          fprintf(stderr, "ERROR: Failed to read single indirect block in the double indirect block!\n"); 
          break; 
        }

        if(single_ib[single_iidx] == 0){
          int new_block = get_free_block(); 
          if(new_block == -1){
            fprintf(stderr, "ERROR: No free blocks are available!\n"); 
            return bytes_written; 
          }
          single_ib[single_iidx] = new_block; 
          ubm.dirty = true; 
          if(block_write(double_ib[double_iidx], &single_ib) != 0){
            fprintf(stderr, "ERROR: Failed to write single indirect block in double indirect block !\n");
            break; 
          }
        }
        block = single_ib[single_iidx]; 
      }

    }

    char blk_buffer[MAX_BLOCK_SIZE];
    if(block_read(block, &blk_buffer) != 0){
      fprintf(stderr, "ERROR: Failed to read block!\n"); 
      return -1; 
    }

    memcpy(blk_buffer + block_off, buffer + bytes_written, byte_write); 
    if(block_write(block, &blk_buffer) != 0){
      fprintf(stderr, "ERROR: Failed to write block!\n");
      return -1; 
    }

    bytes_written += byte_write; 
    fd->offset += byte_write; 
  }

  if((original_offset + bytes_written) >= inode->size){
    inode->size = original_offset + bytes_written; 
    inode->dirty = true; 
  }

  return bytes_written; 
}

int fs_get_filesize(int fildes){
  
  /*
  This function returns the current size of the file referenced by the file descriptor fd. In case fd is
  invalid, the function returns -1.
  */

  if(fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].is_used){
    fprintf(stderr, "ERROR: Invalid file descriptor!\n"); 
    return -1; 
  }

  return inodes[fds[fildes].inode_num].size; 
  
}

int fs_listfiles(char ***files){

  /*
  This function creates and populates an array of all filenames currently known to the file system.
  To terminate the array, your implementation should add a NULL pointer after the last element in
  the array. On success the function returns 0, in the case of an error the function returns -1.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n");
    return -1; 
  }

  int file_count = 0; 
  for(int i = 0; i < MAX_FILES; i++){
    if(root[i].is_used){
      file_count++; 
    }
  }
  
  *files = malloc((file_count + 1) * sizeof(char*)); 
  if(*files == NULL){
    fprintf(stderr, "ERROR: Failure to allocate memory to file array!\n"); 
    return -1; 
  }

  int i = 0; 
  for(int j = 0; j < MAX_FILES; j++){
    if(root[j].is_used){
      //duplicating the names into file array
      (*files)[i] = strdup(root[j].name); 
      if((*files)[i] == NULL){
        fprintf(stderr, "ERROR: Failure to allocate file name!\n"); 
        while(i > 0){
          i--; 
          free((*files)[i]);  
        }
        free(*files); 
        return -1; 
      }
      i++;
    }
  }

  (*files)[file_count] = NULL; //ending with null as told
  return 0; 
}

int fs_lseek(int fildes, off_t offset){

  /*
  This function sets the file pointer (the offset used for read and write operations) associated with
  the file descriptor fd to the argument offset. It is an error to set the file pointer beyond the end of
  the file. To append to a file, one can set the file pointer to the end of a file, for example, by
  calling fs_lseek(fd, fs_get_filesize(fd));. Upon successful completion, a value of
  0 is returned. fs_lseek returns -1 on failure. It is a failure when the file descriptor fd is invalid,
  when the requested offset is larger than the file size, or when offset is less than zero.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n");
    return -1; 
  }

  if(fildes >= MAX_FILDES || fildes < 0 || !fds[fildes].is_used) {
    fprintf(stderr, "ERROR: Invalid file descriptor!\n");
    return -1;
  }

  struct FD* fd = &fds[fildes]; 
  struct inode* inode = &inodes[fd->inode_num]; 

  if(offset < 0 || offset > inode->size){
    fprintf(stderr, "ERROR: Invalid offset!\n");
    return -1;
  }

  fd->offset = offset; 

  return 0; 
}

int fs_truncate(int fildes, off_t length){

  /*
  This function causes the file referenced by fd to be truncated to length bytes in size. If the file
  was previously larger than this new size, the extra data is lost and the corresponding data
  blocks on disk (if any) must be freed. It is not possible to extend a file using fs_truncate.
  When the file pointer is larger than the new length, then it is also set to length (the end of the
  file). Upon successful completion, a value of 0 is returned. fs_lseek returns -1 on failure. It is a
  failure when the file descriptor fd is invalid or the requested length is larger than the file size.
  */

  if(!mounted){
    fprintf(stderr, "ERROR: Disk isn't mounted!\n"); 
    return -1; 
  }

  if(fildes < 0 || fildes >= MAX_FILDES || !fds[fildes].is_used){
    fprintf(stderr, "ERROR: Invalid file descriptor\n");
    return -1; 
  }

  struct FD* fd = &fds[fildes]; 
  struct inode* inode = &inodes[fd->inode_num]; 

  if(length > inode->size){
    fprintf(stderr, "ERROR: Length is larger than file size!\n"); 
    return -1; 
  }

  inode->size = length; 

  size_t block_ptr = MAX_BLOCK_SIZE / sizeof(uint16_t); 
  size_t new_block_count = (length + MAX_BLOCK_SIZE - 1) / MAX_BLOCK_SIZE; 

  for(size_t i = new_block_count; i < 10; i++){
    if(inode->direct_offset[i] != 0){
      clear_bit(inode->direct_offset[i]); 
      ubm.dirty = true; 
      inode->direct_offset[i] = 0;
    }
  }

  if(new_block_count <= 10){
    if(inode->single_indirect != 0){
      uint16_t ib[block_ptr]; 
      block_read(inode->single_indirect, &ib);

      for (size_t i = 0; i < block_ptr; i++) {
        if (ib[i] != 0) {
          clear_bit(ib[i]);
          ubm.dirty = true;
        }
      }
      clear_bit(inode->single_indirect);
      ubm.dirty = true;
      inode->single_indirect = 0;
    }

    if (inode->double_indirect != 0) {
      uint16_t double_ib[block_ptr];
      block_read(inode->double_indirect, &double_ib);

      for (size_t i = 0; i < block_ptr; i++) {
        if (double_ib[i] != 0) {
          uint16_t single_ib[block_ptr];
          block_read(double_ib[i], &single_ib);

          for (size_t j = 0; j < block_ptr; j++) {
            if (single_ib[j] != 0) {
              clear_bit(single_ib[j]);
              ubm.dirty = true;
            }
          }
          clear_bit(double_ib[i]);
          ubm.dirty = true;
          double_ib[i] = 0; // Update the double_ib array
        }
      }
      if (block_write(inode->double_indirect, &double_ib) != 0) {
        fprintf(stderr, "ERROR: Failed to write updated double indirect block!\n");
        return -1;
      }
      inode->double_indirect = 0;
    }
  }

  if (fd->offset > length) {
    fd->offset = length;
  }

  return 0;   
}