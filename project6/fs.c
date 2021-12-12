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

// superblock is block zero
// then we store bit allocation maps
// then we check blocks starting at 2 to find iNode

// need magicNum to check if the file system is correctly formatted
// int magicNumber = 72;

// ORDER: mkfs, stat, open, read, ls, write, close, link, unlink, lseek, unclose

// iNode of current_directory
int current_directory_node;
// there are 256 file_descriptors. these are not persisted though, so no need to store them in disk.
file_descriptor_t fds[256];
// allocated for = 1, not yet allocated = 0
// note that each 
uint8_t block_allocation_map[256];
uint8_t inode_allocation_map[256];

// first block is closed cause the super block is allocated for it
// first block is the super block
void fs_init( void) {
    block_init();
    char readTemp[BLOCK_SIZE];
    block_read(0, readTemp);
    super_block_t* superblock = (super_block_t *)readTemp;
    block_allocation_map[0] = 1;
    // already initialized
    if (superblock->magicNumber == 72) {
        current_directory_node = superblock->root_node_index; 
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
    superblock.magicNumber = 72;

    // set thhe superblock
    // seet the iNode
    
    // zero out the maps and blocks
    bzero_block(zeroTemp);
    for (i = 0; i < FS_SIZE; i++) {
        inode_allocation_map[i] = FALSE;
        block_allocation_map[i] = FALSE;
        block_write(i, zeroTemp);
    }
    block_write(0, (char *)superblock);
    block_allocation_map[0] = 1;
    // let the index of the root dir's iNode = 0
    // first i node is for the root directory
    superblock.root_node_index = 0;
    inode_allocation_map[0] = 1;
    
    char maps[512];

    // allocate maps in the block at index 1
    // inodes and then blocks
    bcopy(inode_allocation_map, maps, 256);
    bcopy(block_allocation_map, (char *)&maps[256], 256);
    block_write(1, maps);
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
        if (!fds[index].inUse) break;
    }
    // all fds in use
    if (index == 256) {
        return -1;
    }
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    (dir_entry_t *)dirEntries[length];
    // read in the current directory from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
    int blockToRead = fs_inodeBlock(current_directory_node);
    block_read(2 + blockToRead, tempBlock);
    bcopy((char *)&tempBlock[(current_directory_node * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)], nodeBlock, sizeof(i_node_t));
    i_node_t *directoryNode = (i_node_t *)&nodeBlock;

    // find the block containing the directory
    blockToRead = directoryNode->blockIndex;
    int size = directoryNode->size;
    block_read(blockToRead, tempBlock);
    int length = size / sizeof(dir_entry_t);
    
    dirEntries = (dir_entry_t *)tempBlock;
    int i;

    bool_t foundString = FALSE;
    short type;
    // do not open directories if not RDONLY
    for (i = 0; i < length; i++) {
        if (same_string(dirEntries[i]->name), fileName)) {
            type = dirEntries[i]->type;
            if (type == DIRECTORY && flags != FS_O_RDONLY) return -1;
            blockToRead = fs_inodeBlock(dirEntries[i]->iNode);
            block_read(2 + blockToRead, tempBlock);
            bcopy((char *)&tempBlock[(dirEntries[i]->iNode * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)], nodeBlock, sizeof(i_node_t));
            i_node_t *tempNode = (i_node_t *)&nodeBlock;
            tempNode->openCount++;
            bcopy((char *)&tempNode, (char *)&tempBlock[(dirEntries[i]->iNode * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);
            foundString = TRUE;
        }
    }
    if (!foundString && flags == FS_O_RDONLY) {
        return -1;
    }
    // if you have not found a new string, then you have found a new file
    if (!foundString) {
        type = FILE_TYPE;
        int iNode = 0;
        // find new iNode
        for (i = 0; i < 256; i++) {
            if (inode_allocation_map[i] != 0xFF) {
                int j;
                for (j = 0; j < 8; j++) {
                    if (inode_allocation_map[i] && (1 << j) == 0)
                        inode_allocation_map[i] | (1 << j);
                        iNode = 8*i + j;
                        break;
                }
            }
        }
        if (i == 256) return -1;
        else {
            blockToRead = fs_inodeBlock(iNode);
            block_read(2 + blockToRead, tempBlock);

            bcopy((char *)&tempBlock[(iNode * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)], nodeBlock, sizeof(i_node_t));
            i_node_t *newNode = (i_node_t *)&nodeBlock;
            newNode->openCount = 1;
            newNode->linkCount = 0;
            newNode->size = 0;
            newNode->type = FILE_TYPE;
            bcopy((char *)&newNode, (char *)&tempBlock[(iNode * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);
        }
        // unable to create new iNode
    }
    fds[index].inUse = TRUE;
    fds[index].name = fileName;
    fds[index].type = type;
    fds[index].flag = flags;
    // copy the new file descriptor into the directory's block
    block_read(directoryNode->blockIndex, tempBlock);
    bcopy((char *)&fds[index], (char *)&tempBlock[length], sizeof(file_descriptor_t));
    block_write(directoryNode->blockIndex, tempBlock);
    return 0;
    }
}

int 
fs_close( int fd) {
    if (!fds[fd].inUse) return -1;
    else {
        fds[fd].inUse = FALSE;
        char tempBlock[BLOCK_SIZE];
        int blockToRead = fs_inodeBlock(fds[fd].iNode);
        block_read(2 + blockToRead, tempBlock);
        i_node_t *node = (i_node_t *)&tempBlock[(fds[fd].iNode * sizeof(i_node_t)) - (blockToRead * BLOCK_SIZE)];
        node->
        if (node->linkCount == 0) {

        }
        bzero(fds[fd], )
    }
    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    if (count == 0) return 0;
    else {
        fds
    }
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

// helper function to find block that contains iNode
// this 2 less than the real index, as the iNodes are allocated starting in the third block
static int fs_inodeBlock(int iNode) {
    return ((iNode * sizeof(i_node_t)) / BLOCK_SIZE);
}


