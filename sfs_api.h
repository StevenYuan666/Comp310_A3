#ifndef SFS_API_H
#define SFS_API_H
#include <stdbool.h>
// You can add more into this file.

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

// Define the global constants
#define DISK_NAME "DISK_FILE"
#define NUMBER_OF_INODES 100
#define NUMBER_OF_BLOCKS 2000
#define BLOCK_SIZE 1024
#define MAGIC_NUMBER 0xACBD0005 // As described in the assigmet instruction
#define ROOT_INODE 0
#define SUPERBLOCK 0
#define FIRST_INODE_BLOCK 1
#define MAXFILENAME 35
#define MAX_FILE_BLOCKS 268

// Define the simplified i-node structure with only one single indirect pointer
typedef struct iNode{
    bool free;
    int mode;
    int linkCount;
    int uId;
    int gId;
    int size;
    int pointer[12];
    int indirectPointer;
} iNode;

// Define the file descriptor which will be used to store the open files in the file table
typedef struct fileDescriptor{
    bool free;
    int iNodeNumber;
    int rwPointer;
} fileDescriptor;

// The block stored the information of the whole file
typedef struct superBlock{
    int magic;
    int blockSize;
    int fileSystemSize;
    int iNodeTableLength;
    int rootINode;
} superBlock;

// Define the directory structure
typedef struct directory{
    bool free;
    int iNodeNumber;
    // include the null terminator here
    char fileName[MAXFILENAME + 1];
} directory;

#endif
