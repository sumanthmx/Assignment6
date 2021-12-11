/*
 * Author(s): <Your name here>
 * COS 318, Fall 2019: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

// need magicNum to check if the file system is correctly formatted
int magicNumber = 72;
int current_directory;
// there are 256 file_descriptors
file_descriptor_t fds[256];
// allocated for = TRUE, not yet allocated = FALSE
bool_t block_allocation_map[FS_SIZE];
bool_t inode_allocation_map[FS_SIZE];
// first block is closed cause the super block is allocated for it
// first block is the super block
void fs_init( void) {
    block_init();
    char readTemp[BLOCK_SIZE];
    block_read(0, readTemp);
    super_block_t* superblock = (super_block_t *)readTemp;
    block_allocation_map[0] = TRUE;
    // already initialized
    if (superblock->magicNumber == magicNumber) {
        current_directory = readTemp->root_directory_index; 
    }
    // not initialized yet
    else {
        fs_mkfs();
    }
    /* More code HERE */
}

int
fs_mkfs( void) {
    char zeroTemp[BLOCK_SIZE];
    super_block_t superblock;
    superblock.magicNumber = magicNumber;

    // set thhe superblock
    // seet the iNode
    
    // zero out the maps and blocks
    bzero_block(zeroTemp);
    for (i = 0; i < FS_SIZE; i++) {
        inode_allocation_map[i] = FALSE;
        block_allocation_map[i] = FALSE;
        block_write(i, zeroTemp);
    }
    block_write(0, superblock);
    // let the index of the root dir's iNode = 0
    // first i node is for the root directory
    superblock.root_directory_index = 0;
    inode_allocation_map[i] = TRUE;

    // set all these fields to null for the file descriptor table
    for (i = 0; i < 256; i++) {
        fd[i].seek = 0;
        fd[i].flag = 0;
        fd[i].inUse = FALSE;
    }
    return -1;
}

int 
fs_open( char *fileName, int flags) {
    int index;
    for (index = 0; index < 256; i++) {
        if (!fds[index].inUse) {
            fds[index].inUse = TRUE;
            break;
        }
    }
    // all fds in use
    if (index == 256) {
        return -1;
    }
    char tempBlock[BLOCK_SIZE];
    // read in the current directory from the disk
    block_read(current_directory, tempBlock);
    

}

int 
fs_close( int fd) {
    if (!fds[fd].inUse) return -1;
    else {
        fds[fd].inUse = FALSE;
        bzero(fds[fd], )
    }
    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    if (count == 0) return 0;

    return -1;
}
    
int 
fs_write( int fd, char *buf, int count) {
    if (count == 0) return 0;
    return -1;
}

int 
fs_lseek( int fd, int offset) {
    return -1;
}

int 
fs_mkdir( char *fileName) {
    return -1;
}

int 
fs_rmdir( char *fileName) {
    return -1;
}

int 
fs_cd( char *dirName) {
    return -1;
}

int 
fs_link( char *old_fileName, char *new_fileName) {
    return -1;
}

int 
fs_unlink( char *fileName) {
    return -1;
}

int 
fs_stat( char *fileName, fileStat *buf) {
    buf->
    return -1;
}

