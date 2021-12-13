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

// size is byteCount
// each node points to 8 blocks
// designed to TAKE 32 bytes: link Count + open Coynt + block Index + blocks + size + short = 
// 2 + 2 + 2 + 2(8) + 4 + 2 = 32
typedef struct i_node {
    uint16_t linkCount; 
    //a
    uint16_t openCount;
    // uint8_t blocksUsed;
    uint16_t blockIndex;
    // blockIndex is for directories mainly... indicates the box that contains the dirEntries
    // bool_t free;
    uint16_t blocks[8]; 
    //a
    int size; 
    // a
    short type; 
    // a

} i_node_t;

typedef struct super_block {
    int magicNumber;
    // i_node_t iNodeStart;
    // int blockCount;
    // int type;
    int root_node_index;
    // file_descriptor_t* dirDescriptor;
} super_block_t;

typedef struct file_descriptor {
    bool_t inUse;
    int iNode;
    int offset;
    int flag;
} file_descriptor_t;

typedef struct dir_entry {
    char name[MAX_FILE_NAME];
    // int nameLength;
    int iNode;
    short type;
    // is directory or not?
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
int fs_inodeBlock(int iNode);
int fs_blockOffset(int iNode, int block);
int inode_index(void);
int block_index(void);

#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
