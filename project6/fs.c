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
file_descriptor_t fds[MAX_FDS];
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

    // preemptively designate the first two blocks to the super block and maps
    
    current_directory_node = 0;

    // create rootDirectory
    make_inode(current_directory_node);
    i_node_t node;
    read_inode(current_directory_node, (char *)&node);
    node.type = DIRECTORY;
    write_inode(current_directory_node, (char *)&node);
    // we know that this is at block_id 0
    newdir_insert(current_directory_node, current_directory_node);

    // create rootDirectory
    

    // set all these fields to null for the file descriptor table
    int i;
    for (i = 0; i < 256; i++) {
        fds[i].offset = 0;
        fds[i].flag = 0;
        fds[i].iNode = -1;
        fds[i].inUse = FALSE;
    }

    char tempBlock[BLOCK_SIZE];
    bzero_block(tempBlock);
    bcopy((unsigned char *)&superBlock, (unsigned char *)tempBlock, sizeof(super_block_t));
    block_write(0, tempBlock);
    // debug line
    // printf("%d\n", sizeof(super_block_t));
    return 0;
    // cases of return -1 come from other failures
    // return -1;
}

int 
fs_open( char *fileName, int flags) {
    // check for open file descriptor
    int index;
    for (index = 0; index < MAX_FDS; index++) {
        if (!fds[index].inUse) break;
    }
    // all fds in use, cannot make a new file descriptor
    if (index == MAX_FDS) {
        return -1;
    }
    // check if in directory
    // do not create node if in RDONLY
    int iNode = findDirectoryEntry(current_directory_node, fileName);
    if (iNode == -1 && flags == FS_O_RDONLY) return -1;
    if (iNode != -1) {
        i_node_t tempNode;
        read_inode(iNode, (char *)&tempNode);
        // illegal to open directory outside RD_ONLY
        if (tempNode.type == DIRECTORY && flags != FS_O_RDONLY) return -1;
        // debug line
        //printf("%d\n", tempNode.openCount);
        tempNode.openCount++;
        write_inode(iNode, (char *)&tempNode);
    }
    else { 
        // we open a file here
        iNode = allocmap_findfree(INODE_MAP);
        // check if freeNode exists or not, and if we can create a new iNode
        if (iNode == -1) return -1;

        i_node_t currentNode;
        read_inode(current_directory_node, (char *)&currentNode);
        // check if there is space to create a new directory entry to begin with
        if (currentNode.size + sizeof(dir_entry_t) > 8 * BLOCK_SIZE) return -1;

        // check for space for blocks
        if (make_inode(iNode) == -1) return -1;

        i_node_t newFileNode;
        read_inode(iNode, (char *)&newFileNode);
        newFileNode.type = FILE_TYPE;
        // new files are opened here, and have open count = 1 to start
        newFileNode.openCount = 1;
        write_inode(iNode, (char *)&newFileNode);

        addDirectoryEntry(current_directory_node, iNode, fileName);
        // make a file
    }

    fds[index].inUse = TRUE;
    fds[index].offset = 0;
    fds[index].iNode = iNode;
    fds[index].flag = flags;
    return index;
}

int 
fs_close( int fd) {
    // if descriptor is not in use, you should NOT close
    if (!fds[fd].inUse) return -1;
    i_node_t node;
    read_inode(fds[fd].iNode, (char *)&node);
    if (node.openCount < 1) return -1;
    // do not open a file which has not been opened
    node.openCount--;
    write_inode(fds[fd].iNode, (char *)&node);
    // if linkCount is zero from unlink and openCount is zero, delete file by 
    if (node.linkCount == 0 && node.openCount == 0) {
        free_inode(fds[fd].iNode);
    }
    fds[fd].inUse = FALSE;

    return 0;
}

int 
fs_read( int fd, char *buf, int count) {
    // fd needs to be a valid descriptor index, and this needs to be in use
    if (fd < 0 || fd > MAX_FDS || !fds[fd].inUse || fds[fd].flag == FS_O_WRONLY) return -1;

    i_node_t node;
    file_descriptor_t descriptor = fds[fd];
    read_inode(descriptor.iNode, (char *)&node);

    // read files only
    if (node.type != FILE_TYPE) return -1;
    int bytesRead = 0;

    while (bytesRead < count && descriptor.offset + bytesRead < node.size) {
        // keep reading bytes through the blocks
        int currentBlock = (descriptor.offset + bytesRead)/BLOCK_SIZE;
        int currentBlockOffset = (descriptor.offset + bytesRead) % BLOCK_SIZE;
        int bytesToRead = count;
       
       /*
        printf("Bytes to read: %d\n", bytesToRead);
        printf("Bytes read: %d\n", bytesRead);
        printf("Block Offset: %d\n", currentBlockOffset);
        printf("Block: %d\n", currentBlock);
        */
        
        if (currentBlockOffset + bytesToRead > BLOCK_SIZE*(currentBlock + 1)) {
            bytesToRead = BLOCK_SIZE * (currentBlock + 1) - currentBlockOffset;
        }
        if (bytesToRead == 0) break;
        char tempBlock[BLOCK_SIZE];
        block_read(fs_dataBlock(node.blocks[currentBlock]), tempBlock);
        bcopy((unsigned char *)&tempBlock[currentBlockOffset], (unsigned char*)buf, bytesToRead);
       
        bytesRead += bytesToRead;
    }
    fds[fd].offset += bytesRead;
    return bytesRead;
}

int 
fs_write( int fd, char *buf, int count) {
    if (fd < 0 || fd > MAX_FDS || !fds[fd].inUse || fds[fd].flag == FS_O_RDONLY) return -1;

    file_descriptor_t descriptor = fds[fd];
    i_node_t node;
    read_inode(descriptor.iNode, (char *)&node);

    if (node.type != FILE_TYPE || descriptor.offset >= MAX_FILE_SIZE) return -1;

    char tempBlock[BLOCK_SIZE];
    int bytesWritten = 0, bytesToWrite = 0, bytesToPad = 0, bytesPadded = 0;
   
    while (node.size + bytesPadded < descriptor.offset) {
        int currentBlock = (node.size + bytesPadded)/BLOCK_SIZE;
        int currentBlockOffset = (node.size + bytesPadded) % BLOCK_SIZE;
        bytesToPad = (descriptor.offset - node.size - bytesPadded);
       
        if (currentBlockOffset + count > BLOCK_SIZE*currentBlock) {
            bytesToPad = BLOCK_SIZE * (currentBlock + 1) - currentBlockOffset;
        }
       
        block_read(fs_dataBlock(node.blocks[currentBlock]), tempBlock);
        bzero((char *)&tempBlock[currentBlockOffset], bytesToPad);
        block_write(fs_dataBlock(node.blocks[currentBlock]), tempBlock);
       
        bytesPadded += bytesToPad;
    }

    while (bytesWritten < count && descriptor.offset + bytesWritten < MAX_FILE_SIZE) {
        int currentBlock = (descriptor.offset + bytesWritten)/BLOCK_SIZE;
        int currentBlockOffset = (descriptor.offset + bytesWritten) % BLOCK_SIZE;
        bytesToWrite = count - bytesWritten;
       
        if (currentBlockOffset + count > BLOCK_SIZE*currentBlock) {
            bytesToWrite = BLOCK_SIZE * (currentBlock + 1) - currentBlockOffset;
        }

        block_read(fs_dataBlock(node.blocks[currentBlock]), tempBlock);
        bcopy((unsigned char*)buf, (unsigned char *)&tempBlock[currentBlockOffset], bytesToWrite);
        block_write(fs_dataBlock(node.blocks[currentBlock]), tempBlock);
       
        bytesWritten += bytesToWrite;
    }
   
    node.size = descriptor.offset + bytesWritten;
    write_inode(descriptor.iNode, (char *)&node);
    fds[fd].offset += bytesWritten + bytesPadded;
    return bytesWritten + bytesPadded;
}

int 
fs_lseek( int fd, int offset) {
    if (!fds[fd].inUse) return -1;
    else {
        fds[fd].offset = offset;
    }
    return 0;
}

int 
fs_mkdir( char *fileName) {
    // child not found in curr directory, so we can continue and create a new directory
    int i = findDirectoryEntry(current_directory_node, fileName);
    if (i != -1) return -1;

    // if no iNodes are available return error
    int childINode = allocmap_findfree(INODE_MAP);
    if (childINode == -1) return -1;

    // if no space for a new dirEntry is available return error
    i_node_t parentNode;
    read_inode(current_directory_node, (char *)&parentNode);
    if (parentNode.size + sizeof(dir_entry_t) > 8 * BLOCK_SIZE) return -1;

    // create directory
    if (make_inode(childINode) == -1) return -1;

    i_node_t childNode;
    read_inode(childINode, (char *)&childNode);
    childNode.type = DIRECTORY;
    write_inode(childINode, (char *)&childNode);
    newdir_insert(current_directory_node, childINode);

    // add directory entry for childNode in parentNode's directory and increment parentNode's size
    addDirectoryEntry(current_directory_node, childINode, fileName);
    return 0;
}

int 
fs_rmdir( char *fileName) {
    // cannot remove parent or current directory or not found file
    if (same_string(fileName, ".") || same_string(fileName, "..")) return -1;

    int iNode = findDirectoryEntry(current_directory_node, fileName);
    if (iNode == -1) return -1;
    i_node_t node;
    read_inode(iNode, (char *)&node);
    // needs to be an empty directory (only . and ..) to be removed
    if (node.type != DIRECTORY || node.size > 2 * sizeof(dir_entry_t)) return -1;
    removeDirectoryEntry(current_directory_node, fileName);
    // remove the node corresponding to this entry
    free_inode(iNode);
    return 0;
    /*
    dir_entry_t last_entry;
    char tempBlock[BLOCK_SIZE];
    int lastBlock = parentNode.size / BLOCK_SIZE;
    int lastOffset = parentNode.size % BLOCK_SIZE;

    if(lastOffset == 0) {
        lastBlock -= 1;
        lastOffset = 7 * sizeof(dir_entry_t);
    } else {
        lastOffset -= sizeof(dir_entry_t);
    }

    block_read(fs_dataBlock(parentNode.blocks[lastBlock]), tempBlock);
    bcopy((unsigned char *)&tempBlock[lastOffset], (unsigned char *)&last_entry, sizeof(dir_entry_t));

    // For each block of a directory, as long as there are entries left to read
    int block;
    int entries = parentNode.size / sizeof(dir_entry_t);
    int nodeFound = 0;
    int currentInode = 0;
    for (block = 0; block < BLOCKS_PER_INODE && entries > 0; block++) {
        char currentBlock[BLOCK_SIZE];
        block_read(fs_dataBlock(parentNode.blocks[block]), currentBlock);

        int entries_in_block = entries;
        if (entries_in_block > DIRECTORY_ENTRIES_PER_BLOCK) entries_in_block = DIRECTORY_ENTRIES_PER_BLOCK;
        int entry_index;

        for (entry_index = 0; entry_index < entries_in_block; entry_index++) {
            dir_entry_t *current_entry = (dir_entry_t*)currentBlock + entry_index;
            if (same_string(current_entry->name, fileName)) {
                // Overwrite this with the last node
                currentInode = current_entry->iNode;
                i_node_t currentNode;
                read_inode(currentInode, (char *)&currentNode);

                // If the directory to remove is not empty, error out
                // if (currentNode.size != 2*sizeof(dir_entry_t)) return -1;

                int i;
                for (i = 0; i < 8; i++) {
                    allocmap_setstatus(BLOCK_MAP, currentNode.blocks[i], NOT_IN_USE);
                }

                bcopy((unsigned char *)&last_entry, (unsigned char *)current_entry, sizeof(dir_entry_t));

                nodeFound = 1;
                break;
            }
        }

        entries -= DIRECTORY_ENTRIES_PER_BLOCK;
        if (nodeFound) {
            break;
        }
    }

    // If we deleted a directory entry by replacing it with the last item,
    if (nodeFound) {
        // Update the parent node
        parentNode.size -= sizeof(dir_entry_t);
        write_inode(current_directory_node, (char *)&parentNode);
        allocmap_setstatus(INODE_MAP, currentInode, NOT_IN_USE);
        return 0;
    }

    return -1;
    */
}

int 
fs_cd( char *dirName) {
    // need to be existing to cd into
    int iNode = findDirectoryEntry(current_directory_node, dirName);
    if (iNode == -1) return -1;

    // cd into directory only
    i_node_t node;
    read_inode(iNode, (char *)&node);
    if (node.type != DIRECTORY) return -1;

    current_directory_node = iNode;
    return 0;
}

int 
fs_link( char *old_fileName, char *new_fileName) {
    // if newFile exists or oldFile does not exist, error (return -1)
    if (findDirectoryEntry(current_directory_node, new_fileName) != -1) return -1;
    int iNode = findDirectoryEntry(current_directory_node, old_fileName);
    if (iNode == -1) return -1;

    // check if we have enough size left for another dirEntry
    i_node_t node;
    read_inode(current_directory_node, (char *)&node);
    if (node.size + sizeof(dir_entry_t) > 8 * BLOCK_SIZE) return -1;

    // increment linkCount of this node
    read_inode(iNode, (char *)&node);
    node.linkCount++;
    write_inode(iNode, (char *)&node);

    // add directoryEntry with new name
    addDirectoryEntry(current_directory_node, iNode, new_fileName);
    return 0;
}

int 
fs_unlink( char *fileName) {
    int iNode = findDirectoryEntry(current_directory_node, fileName);
    if (iNode == -1) return -1;

    i_node_t node;
    read_inode(iNode, (char *)&node);
    // do not call unlink on directories or when linkCount < 1
    if (node.type == DIRECTORY || node.linkCount < 1) return -1;
    node.linkCount--;
    write_inode(iNode, (char *)&node);
    removeDirectoryEntry(current_directory_node, fileName);
    // if last link, then remove file from the file system if no processes are actively opening it
    if (node.linkCount == 0 && node.openCount == 0) free_inode(iNode);
    return 0;
}

int 
fs_stat( char *fileName, fileStat *buf) {
    int inodeNo = findDirectoryEntry(current_directory_node, fileName);

    if (inodeNo >= 0 && inodeNo < FS_SIZE) {
        i_node_t node;
        read_inode(inodeNo, (char *)&node);
        buf->inodeNo = inodeNo;
        buf->type = node.type;
        buf->size = node.size;
        buf->links = node.linkCount;
        if (node.size % BLOCK_SIZE == 0) buf->numBlocks = node.size / BLOCK_SIZE;
        else buf->numBlocks = 1 + (node.size / BLOCK_SIZE);
        return 0;
        // debug 
        // printf("d\n", )
    }
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
                if ((byte & (1 << position)) == 0) return 8*i + position;
            }
        }
    }
    return -1;
}

// translates an entry from the block allocation map to the block in the disk
int fs_dataBlock(int block_id) {
    return DATA_BLOCK_START + block_id;
}
// helper function to find block that contains iNode
int fs_inodeBlock(int iNode) {
    return INODE_BLOCK_START + ((iNode * sizeof(i_node_t)) / BLOCK_SIZE);
}

// helper function to find offset of iNode into its block
int fs_inodeBlockOffset(int iNode) {
    return (iNode * sizeof(i_node_t)) % BLOCK_SIZE;
}

// read in the iNode from the disk
void read_inode(int iNode, char *nodeBlock) {
    char tempBlock[BLOCK_SIZE];
    int blockToRead = fs_inodeBlock(iNode);
    block_read(blockToRead, tempBlock);
    bcopy((unsigned char *)&tempBlock[fs_inodeBlockOffset(iNode)], (unsigned char *)nodeBlock, sizeof(i_node_t));
}

// writes an iNode's information to the disk
void write_inode(int iNode, char *nodeBlock) {
    char tempBlock[BLOCK_SIZE];
    int blockToWrite = fs_inodeBlock(iNode);
    block_read(blockToWrite, tempBlock);
    bcopy((unsigned char *)nodeBlock, (unsigned char *)&tempBlock[fs_inodeBlockOffset(iNode)], sizeof(i_node_t));
    block_write(blockToWrite, tempBlock);
}

// create the parent and child directory entries for a newly made directory
void newdir_insert(int parentNode, int currentNode) {
    i_node_t node;
    read_inode(currentNode, (char *)&node);
    node.size = 2 * sizeof(dir_entry_t);

    char tempBlock[BLOCK_SIZE];
    bzero_block(tempBlock);
    dir_entry_t currDirEntry;
    dir_entry_t parDirEntry;
    char *parentStr = "..";
    char *currStr = ".";
    currDirEntry.iNode = currentNode;
    parDirEntry.iNode = parentNode;

    // copy str null bytes as well
    bcopy((unsigned char*)currStr, (unsigned char*)&currDirEntry.name, strlen(currStr) + 1);
    bcopy((unsigned char*)parentStr, (unsigned char*)&parDirEntry.name, strlen(parentStr) + 1);
    bcopy((unsigned char *)&currDirEntry, (unsigned char *)tempBlock, sizeof(dir_entry_t));
    bcopy((unsigned char *)&parDirEntry, (unsigned char *)&tempBlock[sizeof(dir_entry_t)], sizeof(dir_entry_t));
    block_write(fs_dataBlock(node.blocks[0]), tempBlock);

    write_inode(currentNode, (char *)&node);

}

// returns -1 if not found
// otherwise returns the iNode of the directoryEntry
// helper function, only called on directories
int findDirectoryEntry(int iNode, char *fileName) {
    i_node_t directoryNode;
    read_inode(iNode, (char *)&directoryNode);
    // assumes these blocks ONLY contain entries and that sizeof(dir_entry_t) evenly divides BLOCK_SIZE
    int entries = directoryNode.size / sizeof(dir_entry_t);

    int block;
    // For each block of a directory, as long as there are entries left to read
    for (block = 0; block < BLOCKS_PER_INODE && entries > 0; block++) {
        char currentBlock[BLOCK_SIZE];
        block_read(fs_dataBlock(directoryNode.blocks[block]), currentBlock);

        int entries_in_block = entries;
        if (entries_in_block > DIRECTORY_ENTRIES_PER_BLOCK) entries_in_block = DIRECTORY_ENTRIES_PER_BLOCK;
        int entry_index;

        for (entry_index = 0; entry_index < entries_in_block; entry_index++) {
            dir_entry_t *current_entry = (dir_entry_t*)currentBlock + entry_index;
            if (same_string(current_entry->name, fileName)) {
                return current_entry->iNode;
            }
        }

        entries -= DIRECTORY_ENTRIES_PER_BLOCK;
    }

    // filename not found in current directory
    return -1;
}

// return 0 on success
// return -1 on failure
// ONLY allocate blocks for iNode, do other manipulations in other
// iNode is allocated in map by make_inode
int make_inode(int iNode) {
    int i;
    i_node_t node;
    // default type... change in calling functions
    node.type = 0;
    node.size = 0;
    node.openCount = 0;
    node.linkCount = 1;
    // allocate 8 blocks for the inode
    // TO DO: set blocks one by one
    for (i = 0; i < 8; i++) {
        node.blocks[i] = allocmap_findfree(BLOCK_MAP);
        allocmap_setstatus(BLOCK_MAP, node.blocks[i], IN_USE);
    }
    if (i < 8) {
    // for loop did not finish
        while (i >= 0) {
            allocmap_setstatus(BLOCK_MAP, node.blocks[i], NOT_IN_USE);
            i--;
        }
        return -1;
    }   
    allocmap_setstatus(INODE_MAP, iNode, IN_USE);
    write_inode(iNode, (char *)&node);
    return 0;
}

// add directory entry for childNode in parentNode's directory and increment parentNode's size
// childNode has index iNode
void addDirectoryEntry(int parentiNode, int childiNode, char *fileName) {
    // read and keep local copies of parent and child from their iNodes
    
    i_node_t parentNode;
    read_inode(parentiNode, (char *)&parentNode);
    i_node_t childNode;
    read_inode(childiNode, (char *)&childNode);

    // make dirEntry for child in current_running
    dir_entry_t newEntry;
    newEntry.iNode = childiNode;
    bcopy((unsigned char*)fileName, (unsigned char*)&newEntry.name, strlen(fileName)+1);

    // add dirEntry 
    int block = fs_dataBlock(parentNode.blocks[parentNode.size / BLOCK_SIZE]);
    int offset = parentNode.size % BLOCK_SIZE;
    char tempBlock[BLOCK_SIZE];
    block_read(block, tempBlock);
    bcopy((unsigned char *)&newEntry, (unsigned char *)&tempBlock[offset], sizeof(dir_entry_t));
    block_write(block, tempBlock);

    // update parentNode size
    parentNode.size += sizeof(dir_entry_t);
    write_inode(parentiNode, (char *)&parentNode);
}

// this code is only run when we know that fileName is an entry in parentDirectory
void removeDirectoryEntry(int parentiNode, char *fileName) {
    i_node_t parentNode;
    read_inode(parentiNode, (char *)&parentNode);

    // find the block of the parentNode's last dir entry, and its offset into it
    dir_entry_t last_entry;
    char tempBlock[BLOCK_SIZE];
    int lastBlock = parentNode.size / BLOCK_SIZE;
    int lastOffset = parentNode.size % BLOCK_SIZE;

    // if at start of a block, offset becomes the final entry in the previous block when removing one
    if(lastOffset == 0) {
        lastBlock -= 1;
        lastOffset = 7 * sizeof(dir_entry_t);
    } else {
        lastOffset -= sizeof(dir_entry_t);
    }

    block_read(fs_dataBlock(parentNode.blocks[lastBlock]), tempBlock);
    bcopy((unsigned char *)&tempBlock[lastOffset], (unsigned char *)&last_entry, sizeof(dir_entry_t));

    // find the entry we want to remove
    int block;
    int entries = parentNode.size / sizeof(dir_entry_t);
    int nodeFound = 0;
    int currentInode = 0;

    // loop through all the blocks corresponding to the iNode
    for (block = 0; block < BLOCKS_PER_INODE && entries > 0; block++) {
        char currentBlock[BLOCK_SIZE];
        block_read(fs_dataBlock(parentNode.blocks[block]), currentBlock);

        // iterate through all the entries in the block
        int entries_in_block = entries;
        if (entries_in_block > DIRECTORY_ENTRIES_PER_BLOCK) entries_in_block = DIRECTORY_ENTRIES_PER_BLOCK;
        int entry_index;

        // find the directory entry corresponding to the string
        for (entry_index = 0; entry_index < entries_in_block; entry_index++) {
            dir_entry_t *current_entry = (dir_entry_t*)currentBlock + entry_index;
            if (same_string(current_entry->name, fileName)) {
                // Overwrite this with the last node
                // debug
                // printf("%d", strlen(fileName));
                // printf('\n');
                currentInode = current_entry->iNode;
                i_node_t currentNode;
                read_inode(currentInode, (char *)&currentNode);

                /*
                int i;
                for (i = 0; i < 8; i++) {
                    allocmap_setstatus(BLOCK_MAP, currentNode.blocks[i], NOT_IN_USE);
                }
                */
                // copy in the last dir entry where the one to be removed was at
                bcopy((unsigned char *)&last_entry, (unsigned char *)current_entry, sizeof(dir_entry_t));
                // update in the data block the removed entry
                block_write(fs_dataBlock(parentNode.blocks[block]), currentBlock);
                nodeFound = 1;
                break;
            }
        }

        entries -= DIRECTORY_ENTRIES_PER_BLOCK;
        if (nodeFound) {
            break;
        }
    }
    // Update the parent node and unallocate the inode of the deleted entry
    parentNode.size -= sizeof(dir_entry_t);
    write_inode(parentiNode, (char *)&parentNode);
    // allocmap_setstatus(INODE_MAP, currentInode, NOT_IN_USE);
}

// free blocks and iNode index of the iNode
void free_inode(int iNode) { 
    int i;
    i_node_t node;
    read_inode(iNode, (char *)&node);
    // unallocate 8 blocks for the inode
    // TO DO: set blocks one by one
    for (i = 0; i < 8; i++) {
        allocmap_setstatus(BLOCK_MAP, node.blocks[i], NOT_IN_USE);
    }  
    allocmap_setstatus(INODE_MAP, iNode, NOT_IN_USE);
    write_inode(iNode, (char *)&node);
}
    