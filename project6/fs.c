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
i_node_t current_directory;
// there are 256 file_descriptors
file_descriptor_t fds[256];
bool_t block_allocation_map[FS_SIZE];
// first block is closed cause the super block is allocated for it
// first block is the super block
void fs_init( void) {
    block_init();
    char readTemp[BLOCK_SIZE];
    block_read(0, readTemp);
    super_block_t* superblock = (super_block_t *)readTemp;
    if (superblock->magicNumber == magicNumber) {
        // do shit
    }
    else {
        // do other shit
        fs_mkfs();
    }
    block_allocation_map[0] = FALSE;
    i_node_t root_directory;
    root_directory.linkCount = 0;
    root_directory.
    int i;
    block_write(0, );
    /* More code HERE */
}

int
fs_mkfs( void) {
    char zeroTemp[BLOCK_SIZE];
    super_block_t superblock;
    superblock.magicNumber = magicNumber;
    // superblock.metadata = 

    bzero_block(zeroTemp);
    for (i = 0; i < FS_SIZE; i++) {
        block_write(i, zeroTemp);
    }
    block_write(0, superblock);
    
    // set all these fields to null for the file descriptor table
    for (i = 0; i < 256; i++) {
        fds[i].fd = 0;
    }
    return -1;
}

int 
fs_open( char *fileName, int flags) {
    return -1;
}

int 
fs_close( int fd) {
    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    return -1;
}
    
int 
fs_write( int fd, char *buf, int count) {
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
    return -1;
}

