/*
 * Author(s): <Your name here>
 * COS 318, Fall 2019: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048
#define ROOT_DIRECTORY_INDEX 0

// size is byteCount
// each node points to 8 blocks
typedef struct i_node {
    int linkCount;
    int openCount;
    // int blockIndex;
    // bool_t free;
    int blocks[8];
    int size;
    short type;

} i_node_t;

typedef struct super_block {
    int magicNumber;
    // i_node_t iNodeStart;
    int blockCount;
    // int type;
    int root_node_index;
    // file_descriptor_t* dirDescriptor;
} super_block_t;

typedef struct file_descriptor {
    bool_t inUse;
    int iNode;
    int seek;
    int flag;
} file_descriptor_t;

typedef struct dir_entry {
    char name[MAX_FILE_NAME];
    int nameLength;
    int i_node;
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

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256  // This is the maximum supported "full" path len, eg: /foo/bar/test.txt, rather than the maximum individual filename len.
#endif
