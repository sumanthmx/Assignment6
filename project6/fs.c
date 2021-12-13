/*
 * Author(s): <Your name here>
 * COS 318, Fall 2019: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include "shellutil.h"

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
// use bits: allocated for = 1, not yet allocated = 0

uint8_t block_allocation_map[256];
uint8_t inode_allocation_map[256];

// note that each i_node_t is 32 bytes
// first block is closed cause the super block is allocated for it
// first block is the super block
void fs_init( void) {
    writeInt(sizeof(i_node_t));
    block_init();
    char readTemp[BLOCK_SIZE];
    block_read(0, readTemp);
    super_block_t* superblock = (super_block_t *)readTemp;
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
    char temp[BLOCK_SIZE];
    super_block_t superblock;
    superblock.magicNumber = 72;
    int i;

    // set the superblock
    // seet the iNode
    
    // zero out the maps and blocks
    bzero_block(temp);
    for (i = 0; i < FS_SIZE; i++) {
        block_write(i, temp);
    }
    for (i = 0; i < 256; i++) {
        inode_allocation_map[i] = 0;
        block_allocation_map[i] = 0;
    }
    bcopy((unsigned char*)&superblock, (unsigned char *)&temp, sizeof(super_block_t));
    block_write(0, temp);
    bzero_block(temp);
    // block_allocation_map[0] = 3;
    // allocate a block for bitmaps (1) and superBlock (0)

    // there are FS_SIZE iNodes and each BLOCK has 512 bytes
    // each block has BLOCK_SIZE / sizeof(i_node_t) = 512/32 = 16 iNodes
    // this gives: num of blocks holding iNodes = iNodes / (iNodes per Block) = FS_SIZE / 16 = 128 blocks
    // hence, we allocate 128 blocks for iNodes:
    for (i = 0; i < 16; i++) {
        block_allocation_map[i] = 0xFF;
    }
    block_allocation_map[16] = 3;

    // let the index of the root dir's iNode = 0
    // first i node is for the root directory
    superblock.root_node_index = 0;
    inode_allocation_map[0] = 1;
    
    i_node_t newNode;
    newNode.linkCount = 0;
    newNode.openCount = 0;
    newNode.type = DIRECTORY;
    for (i = 0; i < 8; i++) {
        newNode.blocks[i] = 0;
    }
    // 2 entries for the nodes
    newNode.size = 2 * sizeof(dir_entry_t);
    newNode.blockIndex = (uint16_t) block_index();
    // first iNode is block 
    bcopy((unsigned char*)&newNode, (unsigned char*)&temp, sizeof(i_node_t));
    block_write(2, temp);

    // add the new dir entries
    bzero_block(temp);
    dir_entry_t currDirEntry;
    dir_entry_t parDirEntry;
    bcopy((unsigned char*)&currDirEntry.name, ".", 2);
    bcopy((unsigned char*)&parDirEntry.name, "..", 3);
    currDirEntry.iNode = 0;
    parDirEntry.iNode = 0;
    currDirEntry.type = DIRECTORY;
    parDirEntry.type = DIRECTORY;
    bcopy((unsigned char *)&currDirEntry, (unsigned char *)&temp, sizeof(dir_entry_t));
    bcopy((unsigned char *)&parDirEntry, (unsigned char *)&temp[sizeof(dir_entry_t)], sizeof(dir_entry_t));
    block_write(newNode.blockIndex, tempBlock);

    // finally we allocate our bitmaps
    bzero_block(temp);

    // allocate maps in the block at index 1
    // inodes and then blocks
    bcopy((unsigned char *)&inode_allocation_map, (unsigned char *)&temp, 256);
    bcopy((unsigned char *)&block_allocation_map, (unsigned char *)&temp[256], 256);
    block_write(1, maps);
    // set all these fields to null for the file descriptor table
    for (i = 0; i < 256; i++) {
        fds[i].offset = 0;
        fds[i].flag = 0;
        fds[i].inUse = FALSE;
    }
    return 0;
    // cases of return -1 come from other failures
}

int 
fs_open( char *fileName, int flags) {
    int index;
    int i;
    for (index = 0; index < 256; i++) {
        if (!fds[index].inUse) break;
    }
    // all fds in use
    if (index == 256) {
        return -1;
    }
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    // read in the current directory from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
    int blockToRead = fs_inodeBlock(current_directory_node);
    block_read(2 + blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(current_directory_node, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
    i_node_t *directoryNode = (i_node_t *)&nodeBlock;

    // find the block containing the directory
    blockToRead = directoryNode->blockIndex;
    int size = directoryNode->size;
    block_read(blockToRead, tempBlock);
    int length = size / sizeof(dir_entry_t);
    // dir_entry_t *dirEntries[length];
    // dirEntries = (dir_entry_t *)&tempBlock;
    int iNode = 0;
    bool_t foundString = FALSE;
    short type;
    // do not open directories if not RDONLY
    for (i = 0; i < length; i++) {
        dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + i * sizeof(dir_entry_t));
        if (same_string(dirEntry->name, fileName)) {
            type = dirEntry->type;
            if (type == DIRECTORY && flags != FS_O_RDONLY) return -1;
            blockToRead = fs_inodeBlock(dirEntry->iNode);
            block_read(2 + blockToRead, tempBlock);
            bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
            iNode = dirEntry->iNode;
            i_node_t *tempNode = (i_node_t *)&nodeBlock;
            tempNode->openCount++;
            bcopy((unsigned char *)&tempNode, (unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);
            foundString = TRUE;
        }
    }
    if (!foundString && flags == FS_O_RDONLY) {
        return -1;
    }
    // if you have not found a new string, then you have found a new file
    // if no iNodes are available, cannot create a newFile
    if (!foundString) {
        type = FILE_TYPE;
        // find new iNode
        iNode = inode_index();
        if (iNode == 256) return -1;
        else {
            blockToRead = fs_inodeBlock(iNode);
            block_read(2 + blockToRead, tempBlock);

            bcopy((unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
            i_node_t *newNode = (i_node_t *)&nodeBlock;
            newNode->openCount = 1;
            newNode->linkCount = 0;
            newNode->size = 0;
            newNode->type = FILE_TYPE;
            newNode->blockIndex = 2 + blockToRead;
            bcopy((unsigned char *)&newNode, (unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToRead)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);

            // copy new dirEntry for new file into directory block
            dir_entry_t newEntry;
            bcopy((unsigned char*)&newEntry.name, (unsigned char*)&fileName, MAX_FILE_NAME + 1);
            newEntry.iNode = iNode;
            newEntry.type = FILE_TYPE;
            block_read(directoryNode->blockIndex, tempBlock);
            bcopy((unsigned char *)&newEntry, (unsigned char *)&tempBlock[length], sizeof(dir_entry_t));
            block_write(directoryNode->blockIndex, tempBlock);

        }
        // unable to create new iNode
    }
    fds[index].inUse = TRUE;
    fds[index].offset = 0;
    fds[index].iNode = iNode;
    fds[index].flag = flags;
    
    return 0;
    
}

int 
fs_close( int fd) {
    if (!fds[fd].inUse) return -1;
    else {
        fds[fd].inUse = FALSE;
        char tempBlock[BLOCK_SIZE];
        char nodeBlock[sizeof(i_node_t)];
        int blockToRead = fs_inodeBlock(fds[fd].iNode);
        block_read(2 + blockToRead, tempBlock);
        bcopy((unsigned char *)&tempBlock[fs_blockOffset(fds[fd].iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
        i_node_t *node = (i_node_t *)&nodeBlock;
        // node->
        if (node->linkCount == 0) {

        }
        // bzero(fds[fd], )
    }
    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    if (count == 0) return 0;
    else {
        // what do i read and where???
        // fds[fd]
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

// current directory: "."
// parent directory: ".."
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
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
   
    // read in the current directory from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
    int blockToRead = fs_inodeBlock(current_directory_node);
    block_read(2 + blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(current_directory_node, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
    i_node_t *directoryNode = (i_node_t *)&nodeBlock;

    // find the block containing the directory
    blockToRead = directoryNode->blockIndex;
    int size = directoryNode->size;
    block_read(blockToRead, tempBlock);
    int length = size / sizeof(dir_entry_t);
    // FIX THIS (ask)
    
    int i;
    for (i = 0; i < length; i++) {
        dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + i * sizeof(dir_entry_t));
        if (same_string(dirEntry->name, dirName)) {
            blockToRead = fs_inodeBlock(dirEntry->iNode);
            block_read(2 + blockToRead, tempBlock);
            bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
            i_node_t *node = (i_node_t *)&nodeBlock;
            // check if this is even a (valid) directory
            if (node->type != DIRECTORY) return -1;
            else {
                current_directory_node = dirEntry->iNode;
            }
        }
    }
    // filename not found in current directory 
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
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    
    // read in the current directory from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
    int blockToRead = fs_inodeBlock(current_directory_node);
    block_read(2 + blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(current_directory_node, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
    i_node_t *directoryNode = (i_node_t *)&nodeBlock;

    // find the block containing the directory
    blockToRead = directoryNode->blockIndex;
    int size = directoryNode->size;
    block_read(blockToRead, tempBlock);
    // length is the number of directory entries
    int length = size / sizeof(dir_entry_t);
    //dir_entry_t *dirEntries[length];
    // FIX THIS (ask)
    // dirEntries = (dir_entry_t *)&tempBlock;
    int i;
    int j;
    int count = 0;
    for (i = 0; i < length; i++) {
        dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + i * sizeof(dir_entry_t));
        if (same_string(dirEntry->name, fileName)) {
            buf->inodeNo = dirEntry->iNode;
            blockToRead = fs_inodeBlock(dirEntry->iNode);
            block_read(2 + blockToRead, tempBlock);
            bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
            i_node_t *node = (i_node_t *)&nodeBlock;
            buf->type = node->type;
            buf->links = node->linkCount;
            buf->size = node->size;
            for(j = 0; j < 8; j++) {
                if (node->blocks[j] != 0) count++;
            }
            buf->numBlocks = count;
            return 0;
        }
    }
    // filename not found in current directory
    return -1;
}

// helper function to find block that contains iNode
// this 2 less than the real index, as the iNodes are allocated starting in the third block
int fs_inodeBlock(int iNode) {
    return ((iNode * sizeof(i_node_t)) / BLOCK_SIZE);
}

// helper function to find offset of iNode into its block
// the block passed in here is 2 less than the real index, as the iNodes are allocated starting in the third block
int fs_blockOffset(int iNode, int block) {
    return (iNode * sizeof(i_node_t)) - (block * BLOCK_SIZE);
}

// returns -1 if no free inodes available. otherwise returns index of free inode
// also claims the index and updates the maps (in disk as well) :)
int inode_index(void) {
    int i;
    char tempBlock[BLOCK_SIZE];
    for (i = 0; i < 256; i++) {
        if (inode_allocation_map[i] != 0xFF) {
            int j;
            for (j = 0; j < 8; j++) {
                if (inode_allocation_map[i] && (1 << j) == 0) {
                    inode_allocation_map[i] = inode_allocation_map[i] | (1 << j);
                    block_read(1, tempBlock);
                    bcopy((unsigned char *)&inode_allocation_map, (unsigned char *)&tempBlock, 256);
                    block_write(1, tempBlock);
                    return 8*i + j;
                }
            }
        }
    }
    return -1;
}

// returns -1 if no free blocks available. otherwise returns index of free blocks
// also claims the index and updates the maps (in disk as well) :)
int block_index(void) {
    int i;
    char tempBlock[BLOCK_SIZE];
    for (i = 0; i < 256; i++) {
        if (block_allocation_map[i] != 0xFF) {
            int j;
            for (j = 0; j < 8; j++) {
                if (block_allocation_map[i] && (1 << j) == 0) {
                    block_allocation_map[i] = block_allocation_map[i] | (1 << j);
                    block_read(1, tempBlock);
                    bcopy((unsigned char *)&block_allocation_map, (unsigned char *)&tempBlock[256], 256);
                    block_write(1, tempBlock);
                    return 8*i + j;
                }
            }
        }
    }
    return -1;
}