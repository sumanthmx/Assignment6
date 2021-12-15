/*
 * Author(s): <Your name here>
 * COS 318, Fall 2019: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048
#define ROOT_DIRECTORY_INDEX 0
#define MAX_FILE_NAME 32

#define INODE_MAP 0
#define BLOCK_MAP 1
#define MAP_SIZE 256

#define IN_USE 1
#define NOT_IN_USE 0

#define BLOCKS_PER_INODE 8
#define DIRECTORY_ENTRIES_PER_BLOCK BLOCK_SIZE / sizeof(dir_entry_t)

#define INODE_BLOCK_START 2
#define DATA_BLOCK_START 130
// size is byteCount
// each node points to 8 blocks
// designed to TAKE 32 bytes: 

// BLOCK [0] : superblock, current_directory_node = root_directory
// BLOCK [1] : if inode indexes are available
//             if block indexes are available

// next blocks (starting at 2) are reserved for the iNodes
// we have FS_SIZE number of iNodes... each i node takes sizeof(i_node_t) = 32
// we have num of blocks containing iNodes * BLOCK_SIZE = FS_SIZE * sizeof(i_node_t) 
// num Blocks containing iNodes = FS_SIZE * sizeof(i_node_t) / BLOCK_SIZE = 2048 * 32 / 512 = 128
// set BLOCKS[2 - 129] for i_nodes
// BLOCK[130] onwards is real data blocks

// each file or directory has 8 data blocks allocated to it...

// each file or directory has an iNode
// each directory can contain BLOCK_SIZE * 8 / sizeof(dirEntry) entries

// iNode of current_directory
extern int current_directory_node;

// 2 + 2 + 2(8) + 4 + 2 + 2 + 4 (padding)
typedef struct i_node {
    uint16_t linkCount; 
    uint16_t openCount;
    // uint8_t blocksUsed;

    uint16_t type; 
    

    uint32_t size; 
    // number of directory entries = size / sizeof(dirEntry)

   // from 0 to 7
   // blocks have file names for directories and file contents for files
    uint16_t blocks[8]; 

    // 6 additional bytes to make a total of 32
    uint8_t padding[6];

} i_node_t;

typedef struct super_block {
    uint32_t magicNumber;
    // i_node_t iNodeStart;
    // int blockCount;
    // int type;
    uint32_t rootNodeIndex;
    // file_descriptor_t* dirDescriptor;
} super_block_t;

typedef struct file_descriptor {
    char name[MAX_FILE_NAME];
    bool_t inUse;
    int iNode;
    int offset;
    int flag;
} file_descriptor_t;

// 32 + 2 + 30
typedef struct dir_entry {
    char name[MAX_FILE_NAME];
    
    uint16_t iNode;
    // short type;
    // is directory or not?
    uint8_t extra[30];
    // extra padding
} dir_entry_t;



void fs_init( void);
int fs_mkfs( void);
int fs_open( char *fileName, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *dirName);
int fs_link( char *old_fileName, char *new_fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

// access functions for the block and inode allocation maps in memory
void allocmap_init(int map);
int allocmap_getstatus(int map, int index);
void allocmap_setstatus(int map, int index, int status);
int allocmap_findfree(int map);

void newdir_insert(int parentNode, int currentNode);
//  make_dir_entry(char *entryName, int iNode);
int fs_inodeBlock(int iNode);
int fs_inodeBlockOffset(int iNode);
int inode_index(void);
int block_index(void);
void free_inode(int iNode);
void free_block(int block);
void read_inode(int iNode, char *nodeBlock);
void write_inode(int iNode, char *nodeBlock);
int make_inode(int iNode);
int findDirectoryEntry(int iNode, char *filename);
void addDirectoryEntry(int parentiNode, int childiNode, char *fileName);
// int makeNode(char *nodeBlock, short type, int iNode);
// int findDirectoryEntryBlock(int iNode, char *fileName);
// int findDirectoryEntryOffset(int iNode, char *fileName);
// void removeDirectoryEntry(int iNode, char *fileName);
// void addDirectoryEntry(int directory_node, int entry_node, short type, char *fileName);

#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
