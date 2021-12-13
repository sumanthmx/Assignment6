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

// reading and writing for each map check may be taking a ton of time :(
// CONTINUE AND THINK IN ORDER TO WRITE THROUGH

// superblock is block zero
// then we store bit allocation maps
// then we check blocks starting at 2 to find iNode

// need magicNum to check if the file system is correctly formatted
// int magicNumber = 72;

// ORDER: mkfs, stat, open, read, ls, write, close, link, unlink, lseek, unclose

// note that each i_node_t is 32 bytes
// first block is closed cause the super block is allocated for it
// first block is the super block
void fs_init( void) {
    // writeInt(sizeof(i_node_t));
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
    
    // finally we allocate our bitmaps
    bzero_block(temp);

    // allocate maps in the block at index 1
    // inodes and then blocks
    bcopy((unsigned char *)&inode_allocation_map, (unsigned char *)&temp, 256);
    bcopy((unsigned char *)&block_allocation_map, (unsigned char *)&temp[256], 256);
    block_write(1, temp);
    i_node_t newNode;
    newNode.linkCount = 0;
    newNode.openCount = 0;
    newNode.type = DIRECTORY;

    for (i = 0; i < 8; i++) {
        newNode.blocks[i] = block_index();
        // will always acquire a block here
    }
    // 2 entries for the nodes
    newNode.size = 2 * sizeof(dir_entry_t);
    newNode.paddingField = 0;

    // first iNode is block 
    bcopy((unsigned char*)&newNode, (unsigned char*)&temp, sizeof(i_node_t));
    block_write(2, temp);

    // add the new dir entries
    bzero_block(temp);
    dir_entry_t currDirEntry;
    dir_entry_t parDirEntry;
    char parentStr[2] = "..";
    char currStr[1] = ".";
    // char nodeBlock[sizeof(i_node_t)];
    bcopy((unsigned char*)&currStr, (unsigned char*)&currDirEntry.name, strlen(currStr));
    bcopy((unsigned char*)&parentStr, (unsigned char*)&parDirEntry.name, strlen(parentStr));
    currDirEntry.iNode = 0;
    parDirEntry.iNode = 0;
    currDirEntry.type = DIRECTORY;
    parDirEntry.type = DIRECTORY;
    bcopy((unsigned char *)&currDirEntry, (unsigned char *)&temp, sizeof(dir_entry_t));
    bcopy((unsigned char *)&parDirEntry, (unsigned char *)&temp[sizeof(dir_entry_t)], sizeof(dir_entry_t));
    block_write(newNode.blocks[0], temp);

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


    int iNode = 0;
    bool_t foundString = FALSE;
    short type;
    // do not open directories if RDONLY


    // find the block containing the directory [FIXED]
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries and print em!
    while (sum < size) {
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + b * sizeof(dir_entry_t));
            if (same_string(dirEntry->name, fileName)) {
                type = dirEntry->type;
                // check this RDONLY THING
                if (type == DIRECTORY || flags == FS_O_RDONLY) return -1;
                blockToRead = fs_inodeBlock(dirEntry->iNode);
                block_read(2 + blockToRead, tempBlock);
                bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
                iNode = dirEntry->iNode;
                i_node_t *tempNode = (i_node_t *)&nodeBlock;
                tempNode->openCount++;
                bcopy((unsigned char *)&tempNode, (unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], sizeof(i_node_t));
                block_write(2 + blockToRead, tempBlock);
                foundString = TRUE;
                break;
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
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
        if (iNode == -1) return -1;
        else {
            blockToRead = fs_inodeBlock(iNode);
            block_read(2 + blockToRead, tempBlock);

            bcopy((unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
            i_node_t *newNode = (i_node_t *)&nodeBlock;
            newNode->openCount = 1;
            newNode->linkCount = 0;
            newNode->size = 0;
            newNode->type = FILE_TYPE;
            newNode->paddingField = 0;
            // allocate 8 blocks
            for (i = 0; i < 8; i++) {
                int j = block_index();
                if (j == -1) {
                    int k = 0;
                    while (k < i) {
                        free_block(newNode->blocks[k]);
                        k++;
                    }
                    free_inode(iNode);
                    return -1;
                    // if not enough blocks are free, free the inode and other blocks
                }
                else {
                    newNode->blocks[i] = j;
                }
            }
            // files do NOT need their own blocks of directoryEntries, so we just make this the block its contained in
            bcopy((unsigned char *)&newNode, (unsigned char *)&tempBlock[fs_blockOffset(iNode, blockToRead)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);

            // copy new dirEntry for new file into directory block[FIX]
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
        if (node->openCount < 1) return -1;
        node->openCount--;
        // if linkCount is zero, remove file
        if (node->linkCount == 0) {
            free_inode(fds[fd].iNode);
        }
        // bzero(fds[fd], )
    }
    return -1;
}

int 
fs_read( int fd, char *buf, int count) {
    if (count == 0) return 0;
    if (!fds[fd].inUse || fds[fd].flag == FS_O_WRONLY) return -1;
    // if unable to read anything into the buffer, do not read and return failure
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    int blockToRead = fs_inodeBlock(fds[fd].iNode);
    block_read(2 + blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(fds[fd].iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
    i_node_t *node = (i_node_t *)&nodeBlock;
    if (count > node->size - fds[fd].offset) return -1;
    int index = fds[fd].offset / BLOCK_SIZE;
    int offsetIntoBlock = fds[fd].offset % BLOCK_SIZE;
    int amountToRead = count;
    int amountReading = BLOCK_SIZE - offsetIntoBlock;
    int amountRead = 0;
    while (amountToRead > 0) {
        if (amountToRead < amountReading) {
            amountReading = amountToRead;
        }
        block_read(node->blocks[index], tempBlock);
        bcopy((unsigned char *)&tempBlock[offsetIntoBlock], (unsigned char *)&buf[amountRead], amountReading);
        amountToRead -= amountReading;
        amountRead += amountReading;
        offsetIntoBlock = 0;
        index += 1;
        amountReading = BLOCK_SIZE;
    }
    // read bytes here
    fds[fd].offset += count;
    // what do i read and where???
    // fds[fd]
    return 0;
}
    
int // edit
fs_write( int fd, char *buf, int count) {
    if (count == 0) return 0;
    if (!fds[fd].inUse || fds[fd].flag == FS_O_RDONLY) return -1;
    // if unable to continue writing due to space issues, do not write and return failure
    if (count > 8*BLOCK_SIZE - fds[fd].offset) return -1;
    else {
        int i;
        int index;
        int offsetIntoBlock;
        int amountToWrite;
        int amountWriting;
        int amountWritten;
        char tempBlock[BLOCK_SIZE];
        char nodeBlock[sizeof(i_node_t)];
        int blockToRead = fs_inodeBlock(fds[fd].iNode);
        block_read(2 + blockToRead, tempBlock);
        bcopy((unsigned char *)&tempBlock[fs_blockOffset(fds[fd].iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
        i_node_t *node = (i_node_t *)&nodeBlock;
        // update size if necessary
        if (node->size < fds[fd].offset + count) {
            node->size = fds[fd].offset + count;
            bcopy((unsigned char *)&nodeBlock, (unsigned char *)&tempBlock[fs_blockOffset(fds[fd].iNode, blockToRead)], sizeof(i_node_t));
            block_write(2 + blockToRead, tempBlock);
        }
        // pad with null chars until the offset is reached
        if (node->size < fds[fd].offset) {
            index = node->size / BLOCK_SIZE;
            offsetIntoBlock = node->size % BLOCK_SIZE;
            amountToWrite = fds[fd].offset - node->size;
            amountWriting = BLOCK_SIZE - offsetIntoBlock;
            amountWritten = 0;
            while (amountToWrite > 0) {
                if (amountToWrite < amountWriting) {
                    amountWriting = amountToWrite;
                } 
                // maintain what comes before node->size in the block by reading first
                block_read(node->blocks[index], tempBlock);
                i = offsetIntoBlock;
                for (i = 0; i < BLOCK_SIZE; i++) {
                    tempBlock[i] = '\0';
                }
                block_write(node->blocks[index], (char *)&tempBlock[offsetIntoBlock]);
                amountToWrite -= amountWriting;
                amountWritten += amountWriting;
                offsetIntoBlock = 0;
                index += 1;
                amountWriting = BLOCK_SIZE;
            }
        }
        // write bytes from buf into the blocks
        index = fds[fd].offset / BLOCK_SIZE;
        offsetIntoBlock = fds[fd].offset % BLOCK_SIZE;
        amountToWrite = count;
        amountWriting = BLOCK_SIZE - offsetIntoBlock;
        amountWritten = 0;
        while (amountToWrite > 0) {
            if (amountToWrite < amountWriting) {
                amountWriting = amountToWrite;
            }
            block_read(node->blocks[index], tempBlock);
            bcopy((unsigned char *)&buf[amountWritten], (unsigned char *)&tempBlock[offsetIntoBlock], amountWriting);
            block_write(node->blocks[index], (char *)&tempBlock[offsetIntoBlock]);
            amountToWrite -= amountWriting;
            amountWritten += amountWriting;
            offsetIntoBlock = 0;
            index += 1;
        }
        // update offset
        fds[fd].offset += count;
    }
    return 0;
}

int 
fs_lseek( int fd, int offset) {
    if (!fds[fd].inUse) return -1;
    else {
        fds[fd].offset = offset;
    }
    return 0;
}

// current directory: "."
// parent directory: ".."
int 
fs_mkdir( char *fileName) {
    char tempBlock[BLOCK_SIZE];
    char nodeBlock[sizeof(i_node_t)];
    int i;
    // read in the current directory from the disk
    // add 2 cause first is super (no iNodes in 1st one) and second is maps
    int blockToRead = fs_inodeBlock(current_directory_node);
    block_read(2 + blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_blockOffset(current_directory_node, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
    i_node_t *directoryNode = (i_node_t *)&nodeBlock;

    // find the block containing the directory entries[FIX]
    // put in a new directory entry in current directory[FIX]
    blockToRead = directoryNode->blockIndex;
    int size = directoryNode->size;
    if (size + sizeof(i_node_t) > 8 * BLOCK_SIZE) return -1;
    // cannot add more i_nodes
    
    block_read(blockToRead, tempBlock);
    int length = size / sizeof(dir_entry_t);
    // check if fileName already exists
    for (i = 0; i < 8; i++) {
        dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + i * sizeof(dir_entry_t));
        if (same_string(dirEntry->name, fileName)) {
            return -1;
        }
    }
    // child not found in curr directory, so we can continue and create a new directory
    bzero_block(tempBlock);
    
    dir_entry_t currDirEntry;
    dir_entry_t parDirEntry;
    char parentStr[2] = "..";
    char currStr[1] = ".";
    currDirEntry.iNode = inode_index();
    // dont continue if no free inodes
    if (currDirEntry.iNode == -1) {
        return -1;
    }
    i_node_t newNode;
    newNode.linkCount = 0;
    newNode.openCount = 0;
    newNode.type = DIRECTORY;
    for (i = 0; i < 8; i++) {
        int j = block_index();
        if (j == -1) {
            int k = 0;
            while (k < i) {
                free_block(newNode.blocks[k]);
                k++;
            }
            free_inode(currDirEntry.iNode);
            return -1;
            // fail to acquire sufficient blocks means no newNode
            // make sure to free preemptively acquired inode too
        }
        else {
            newNode.blocks[i] = j;
        }
    }
    // 2 entries for the nodes
    newNode.size = 2 * sizeof(dir_entry_t);

    parDirEntry.iNode = current_directory_node;
    currDirEntry.type = DIRECTORY;
    parDirEntry.type = DIRECTORY;
    bcopy((unsigned char*)&currStr, (unsigned char*)&currDirEntry.name, strlen(currStr));
    bcopy((unsigned char*)&parentStr, (unsigned char*)&parDirEntry.name, strlen(parentStr));
    bcopy((unsigned char *)&currDirEntry, (unsigned char *)&temp, sizeof(dir_entry_t));
    bcopy((unsigned char *)&parDirEntry, (unsigned char *)&temp[sizeof(dir_entry_t)], sizeof(dir_entry_t));
    block_write(newNode.blocks[0], tempBlock);
    return 0;
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

    // find the block containing the directory [FIXED]
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    // int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries and check for this new dir to cd into
    while (sum < size) {
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + b * sizeof(dir_entry_t));
            if (same_string(dirEntry->name, dirName)) {
                blockToRead = fs_inodeBlock(dirEntry->iNode);
                block_read(2 + blockToRead, tempBlock);
                bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
                i_node_t *node = (i_node_t *)&nodeBlock;
                // check if this is even a (valid) directory
                if (node->type != DIRECTORY) return -1;
                else {
                    current_directory_node = dirEntry->iNode;
                    return 0;
                }
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
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

    // find the block containing the directory [FIXED]
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int size = directoryNode->size;
    int length = size / sizeof(dir_entry_t);
    int entriesPerFullBlock = BLOCK_SIZE / sizeof(dir_entry_t);
    int entriesInBlock = entriesPerFullBlock;
    int sum = 0;
    int a = 0;
    int b;
    // find all directory entries and print em!
    while (sum < size) {
        if (size - sum < sizeof(dir_entry_t) * entriesPerFullBlock) {
            entriesInBlock = (size - sum) / sizeof(dir_entry_t);
        }
        blockToRead = directoryNode->blocks[a];
        block_read(blockToRead, tempBlock);
        for (b = 0; b < entriesInBlock; b++) {
            sum += sizeof(dir_entry_t);
            dir_entry_t *dirEntry = (dir_entry_t *)(&tempBlock + b * sizeof(dir_entry_t));
            if (same_string(dirEntry->name, fileName)) {
                buf->inodeNo = dirEntry->iNode;
                blockToRead = fs_inodeBlock(dirEntry->iNode);
                block_read(2 + blockToRead, tempBlock);
                bcopy((unsigned char *)&tempBlock[fs_blockOffset(dirEntry->iNode, blockToRead)], (unsigned char *)&nodeBlock, sizeof(i_node_t));
                i_node_t *node = (i_node_t *)&nodeBlock;
                buf->type = node->type;
                buf->links = node->linkCount;
                buf->size = node->size;
                if (node->size % BLOCK_SIZE == 0) {
                    buf->numBlocks = node->size / BLOCK_SIZE;
                }
                else buf->numBlocks = node->size / BLOCK_SIZE + 1;
                return 0;
            }
        }
        a++;
        entriesInBlock = entriesPerFullBlock;
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

// unallocate an iNode
void free_inode(int iNode) {
    char tempBlock[BLOCK_SIZE];
    int j = iNode % 8;
    int i = iNode - j / 8;
    inode_allocation_map[i] = inode_allocation_map[i] ^ (1 << j);
    block_read(1, tempBlock);
    bcopy((unsigned char *)&inode_allocation_map, (unsigned char *)&tempBlock, 256);
    block_write(1, tempBlock);
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

// unallocate a block and zero out its contents
void free_block(int block) {
    char tempBlock[BLOCK_SIZE];
    int j = block % 8;
    int i = block - j / 8;
    block_allocation_map[i] = block_allocation_map[i] ^ (1 << j);
    block_read(1, tempBlock);
    bcopy((unsigned char *)&block_allocation_map, (unsigned char *)&tempBlock[256], 256);
    block_write(1, tempBlock);
    bzero_block(tempBlock);
    block_write(block, tempBlock);
}
