/*
 * Author(s): <Your name here>
 * COS 318, Fall 2019: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"
#include "assert.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

// iNode of current_directory
int current_directory_node;
// there are 256 file_descriptors. these are not persisted though, so no need to store them in disk.
file_descriptor_t fds[256];
// allocated for = 1, not yet allocated = 0

// DO NOT ACCESS DIRECTLY (only through helper functions) these are stored in BLOCK 1
// uint8_t block_allocation_map[MAP_SIZE];
// uint8_t inode_allocation_map[MAP_SIZE];

void 
fs_init( void) {
    // writeInt(sizeof(dir_entry_t));
    block_init();
    char readTemp[BLOCK_SIZE];
    block_read(0, readTemp);
    super_block_t* superBlock = (super_block_t *)readTemp;
    // already initialized
    if (superBlock->magicNumber == 72) {
        current_directory_node = superBlock->rootNodeIndex; 
    }
    // not initialized yet
    else {
        fs_mkfs();
    }
}

int
fs_mkfs( void) {
    // initialize superBlock with magicNumber 72 and set rootNode equal to zero
    super_block_t superBlock;
    superBlock.magicNumber = 72;
    superBlock.rootNodeIndex = 0;

    // initialize bitMaps
    allocmap_init(INODE_MAP);
    allocmap_init(BLOCK_MAP);
    // first inode and block go to root directory and superblock respectively
    current_directory_node = 0;
    fs_mkdir("/");

    // create rootDirectory
    dir_entry_t rootDirEntry;
    // bcopy(".")
    // update 
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
    // makes a new directory in current directory
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

// initializes a map
void allocmap_init(int map) {
    int offset = 0;
    if (map == BLOCK_MAP) offset = MAP_SIZE;
    char tempBlock[BLOCK_SIZE];
    block_read(1, tempBlock);
    bzero(tempBlock + offset, MAP_SIZE);
    block_write(1, tempBlock);
}
int allocmap_getstatus(int map, int index) {
    int offset = 0;
    if (map == BLOCK_MAP) offset = MAP_SIZE;
    char tempBlock[BLOCK_SIZE];
    block_read(1, tempBlock);

    // get bit corresponding to the index
    int byte = tempBlock[offset + index / 8];
    return (byte >> (index % 8)) & 1;
}
void allocmap_setstatus(int map, int index, int status) {
    int offset = 0;
    if (map == BLOCK_MAP) offset = MAP_SIZE;
    char tempBlock[BLOCK_SIZE];
    block_read(1, tempBlock);

    // set bit corresponding to the index
    int byte = tempBlock[offset + index / 8];
    int position = index % 8;

    // zero out the position bit in byte, and then restore that bit if status is active
    tempBlock[offset + index / 8] = (byte & (~(1 << position))) | (status << position);
    block_write(1, tempBlock);
}

// return -1 if none free
int allocmap_findfree(int map) {
    int offset = 0;
    if (map == BLOCK_MAP) offset = MAP_SIZE;
    char tempBlock[BLOCK_SIZE];
    block_read(1, tempBlock);
    int i;
    for (i = 0; i < MAP_SIZE; i++) {
        uint8_t byte = (uint8_t) tempBlock[offset + i];
        if (byte != 0xFF) {
            int position;
            for (position = 0; position < 8; position++) {
                if ((byte && (1 << position)) == 0) return 8*i + position;
            }
        }
    }
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
    // read in the iNode from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
void read_inode(int iNode, char *nodeBlock) {
    char tempBlock[BLOCK_SIZE];
    int blockToRead = 2 + fs_inodeBlock(iNode);
    block_read(blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToRead - 2)], (unsigned char *)nodeBlock, sizeof(i_node_t));
}
// writes an iNode's information to the disk
void write_inode(int iNode, char *nodeBlock) {
    char tempBlock[BLOCK_SIZE];
    int blockToWrite = 2 + fs_inodeBlock(iNode);
    block_read(blockToWrite, tempBlock);
    bcopy((unsigned char *)nodeBlock, (unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToWrite - 2)], sizeof(i_node_t));
    block_write(blockToWrite, tempBlock);
}

/*
// returns -1 if not found
// otherwise returns the iNode of the directoryEntry
// helper function, only called on directories
int findDirectoryEntry(int iNode, char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    read_inode(iNode, nodeBlock);
    i_node_t *directoryNode = (i_node_t *)nodeBlock;
    int blockToRead;
    // find the block containing the directory [
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    // int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries 
    while (sum < size) {
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock[b * sizeof(dir_entry_t)]);
            if (same_string(dirEntry->name, fileName)) {
                return dirEntry->iNode;
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
    }
    // filename not found in current directory
    return -1;
}

// returns -1 if not found
// otherwise returns the block in which the directoryEntry is stored in
// helper function, only called on directories
int findDirectoryEntryBlock(int iNode, char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    read_inode(iNode, nodeBlock);
    i_node_t *directoryNode = (i_node_t *)nodeBlock;
    int blockToRead;
    // find the block containing the directory [
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    // int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries 
    while (sum < size) {
        assert(a < 8);
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock[b * sizeof(dir_entry_t)]);
            if (same_string(dirEntry->name, fileName)) {
                return blockToRead;
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
    }
    // filename not found in current directory
    return -1;
}

// returns -1 if not found
// otherwise returns the offset into the block in which the directoryEntry is stored in
// helper function, only called on directories
int findDirectoryEntryOffset(int iNode, char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    read_inode(iNode, nodeBlock);
    i_node_t *directoryNode = (i_node_t *)nodeBlock;
    int blockToRead;
    // find the block containing the directory [
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    // int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries 
    while (sum < size) {
        assert(a < 8);
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock[b * sizeof(dir_entry_t)]);
            if (same_string(dirEntry->name, fileName)) {
                return b * sizeof(dir_entry_t);
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
    }
    // filename not found in current directory
    return -1;
}

// remove directory entry of name fileName
// IMPORTANT: note, this method also increments the node sizes too
void removeDirectoryEntry(int iNode, char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char secondTempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    char entryBlock[sizeof(dir_entry_t)];
    // we update the directoryNode in memory
    // update size (and possibly last index) of curr directory
    // need these to be changed as the final entry is where the second to last one ends
    read_inode(iNode, nodeBlock);
    i_node_t *directoryNode = (i_node_t *)nodeBlock;
    if (directoryNode->size % BLOCK_SIZE == 0) directoryNode->lastBlockIndex--;
    directoryNode->size -= sizeof(dir_entry_t);
    write_inode(iNode, nodeBlock);

    int incisionBlock = findDirectoryEntryBlock(iNode, fileName);
    int finalBlock = directoryNode->blocks[directoryNode->lastBlockIndex];

    // copy the final entry in the directory and move it to where the lost one was
    block_read(incisionBlock, tempBlock);
    block_read(finalBlock, secondTempBlock);
    bcopy((unsigned char*)&secondTempBlock[directoryNode->size % BLOCK_SIZE], (unsigned char*)entryBlock, sizeof(dir_entry_t));
    bcopy((unsigned char*)entryBlock, (unsigned char*)&tempBlock[findDirectoryEntryOffset(iNode, fileName)], sizeof(dir_entry_t));
    block_write(incisionBlock, tempBlock);

    // then remove the final entry from the directory
    bzero(entryBlock, sizeof(dir_entry_t));
    bcopy((unsigned char*)entryBlock, (unsigned char*)&secondTempBlock[directoryNode->size % BLOCK_SIZE], sizeof(dir_entry_t));
    block_write(finalBlock, secondTempBlock);
}

// add directory entry of name fileName into directory_node
// IMPORTANT: note, this method also increments the size of directory_node too
void addDirectoryEntry(int directory_node, int entry_node, short type, char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    // finally add directory entry for current directory
    dir_entry_t nextEntry;
    nextEntry.iNode = entry_node;
    nextEntry.type = type;
    bcopy((unsigned char*)&fileName, (unsigned char*)&nextEntry.name, strlen(fileName));

    // insert entry (size was defined earlier)
    read_inode(directory_node, nodeBlock);
    i_node_t *directoryNode = (i_node_t *)nodeBlock;
    // debug line
    // printf("%d\n", directoryNode->type);
    block_read(directoryNode->blocks[directoryNode->lastBlockIndex], tempBlock);
    bcopy((unsigned char *)&nextEntry, (unsigned char *)&tempBlock[directoryNode->size % BLOCK_SIZE], sizeof(dir_entry_t));
    block_write(directoryNode->blocks[directoryNode->lastBlockIndex], tempBlock);

    // update size (and possibly last index) of curr directory
    read_inode(directory_node, nodeBlock);
    directoryNode = (i_node_t *)nodeBlock;
    directoryNode->size += sizeof(dir_entry_t);
    if (directoryNode->size % BLOCK_SIZE == 0) directoryNode->lastBlockIndex++;
    write_inode(directory_node, nodeBlock);
    
}
*/