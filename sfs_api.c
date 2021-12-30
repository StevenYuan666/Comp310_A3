// include the necessary libraries and the header files
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include "disk_emu.h"
#include "sfs_api.h"
#include <stdbool.h>

// Create an i-nodes table
iNode iNodeTable[NUMBER_OF_INODES];

// Create a file-descriptors table
fileDescriptor fdTable[NUMBER_OF_INODES];

directory directoryTable[NUMBER_OF_INODES];

superBlock sb;

int bitMap[NUMBER_OF_BLOCKS];

// index of the next file
int location = 0;

// flush system cache to the disk
void flush(){
    // Initialize an empty buffer
    char buffer[20000];
    memset(buffer, 0, sizeof(buffer));

    // flush the super block
    memcpy(buffer, &sb, sizeof(sb));
    write_blocks(0, 1, buffer);

    // empty the buffer
    memset(buffer, 0, sizeof(buffer));

    // flush the i-nodes table
    memcpy(buffer, iNodeTable, sizeof(iNodeTable));
    write_blocks(1, sb.iNodeTableLength, buffer);

    // empty the buffer
    memset(buffer, 0, sizeof(buffer));

    // flush the directory table
    memcpy(buffer, directoryTable, sizeof(directoryTable));
    write_blocks(iNodeTable[sb.rootINode].pointer[0], 12, buffer);

    // empty the buffer
    memset(buffer, 0, sizeof(buffer));

    // flush the bit map
    memcpy(buffer, bitMap, sizeof(bitMap));
    write_blocks(NUMBER_OF_BLOCKS - (sizeof(bitMap) / BLOCK_SIZE + 1), (sizeof(bitMap) / BLOCK_SIZE + 1), buffer);
}

void mksfs(int fresh){
    if(fresh){
        // Calling the given function to initialize a fresh disk
        init_fresh_disk(DISK_NAME, BLOCK_SIZE, NUMBER_OF_BLOCKS);

        // initialize the memory when init a fresh disk
        for(int i = 0; i < NUMBER_OF_INODES; i ++){
            iNodeTable[i].free = true;
            fdTable[i].free = true;
            directoryTable[i].free = true;
        }
        // Initialize the bitmap
        for(int i = 0; i < NUMBER_OF_BLOCKS; i ++){
            bitMap[i] = 1;
        }
        // Store the relative file information in the super block
        sb.blockSize = BLOCK_SIZE;
        sb.fileSystemSize = NUMBER_OF_BLOCKS;
        sb.iNodeTableLength = sizeof(iNodeTable) / BLOCK_SIZE + 1;
        sb.magic = MAGIC_NUMBER;
        sb.rootINode = ROOT_INODE;

        // Update the information stored inn the bitmap
        bitMap[0] = 0;
        bitMap[NUMBER_OF_BLOCKS - 1] = 0;
        for(int i = (NUMBER_OF_BLOCKS - (sizeof(bitMap) / BLOCK_SIZE + 1)); i < NUMBER_OF_BLOCKS; i ++){
            bitMap[i] = 0;
        }
        for(int i = 1; i <= sb.iNodeTableLength; i ++){
            bitMap[i] = 0;
        }
        // Initialize the iNodeTable
        iNodeTable[sb.rootINode].free = false;
        int counter = 0;
        for(int i = (sb.iNodeTableLength + 1); i <= (sb.iNodeTableLength + 12); i ++){
            bitMap[i] = 0;
            iNodeTable[sb.rootINode].pointer[counter] = i;
            counter ++;
            if(counter == 12){
                break;
            }
        }
        iNodeTable[sb.rootINode].indirectPointer = -1;
        iNodeTable[sb.rootINode].linkCount = 12;
        iNodeTable[sb.rootINode].size = sizeof(directoryTable);
        iNodeTable[sb.rootINode].mode = 0;
    }
    else{
        // Open the existed disk file
        init_disk(DISK_NAME, BLOCK_SIZE, NUMBER_OF_BLOCKS);
        
        // initialize the memory when load an existed disk
        for(int i = 0; i < NUMBER_OF_INODES; i ++){
            fdTable[i].free = true;
        }
        // Initialize an empty buffer
        char buffer[20000];
        memset(buffer, 0, sizeof(buffer)); 
        
        // load the super block first from the disk file
        read_blocks(0, 1, buffer);
        memcpy(&sb, buffer, sizeof(sb));

        // load the i-nodes table
        read_blocks(1, sb.iNodeTableLength, buffer);
        memcpy(iNodeTable, buffer, sizeof(iNodeTable));

        // load directories table(data blocks)
        read_blocks(iNodeTable[sb.rootINode].pointer[0], 12, buffer);
        memcpy(directoryTable, buffer, sizeof(directoryTable));

        // load the bitmap
        read_blocks((NUMBER_OF_BLOCKS - (sizeof(bitMap) / BLOCK_SIZE + 1)), (sizeof(bitMap) / BLOCK_SIZE + 1), buffer);
        memcpy(bitMap, buffer, sizeof(bitMap));
    }
}

// Find the next available file name
int sfs_getnextfilename(char *fileName){
    // Use a global variable location so that we can know where should we start
    for(int counter = location; counter < NUMBER_OF_INODES; counter ++){
        if(!directoryTable[counter].free){
            strcpy(fileName, directoryTable[counter].fileName);
            location = counter + 1;
            return 1;
        }
    }
    // Restore the locationn
    location = 0;
    return 0;
}

int sfs_getfilesize(const char* filePath){
    for(int i = 0; i < NUMBER_OF_INODES; i ++){
        if(!directoryTable[i].free){
            // Find such file
            if(strcmp(directoryTable[i].fileName, filePath) == 0){
                int index = directoryTable[i].iNodeNumber;
                return iNodeTable[index].size;
            }
        }
    }
    // Return -1 if we don't find
    return -1;
}

int addTofdTable(int iNodeNumber){
    // Add a file descriptor to the fdTable
    for(int i = 0; i < NUMBER_OF_INODES; i ++){
        if(fdTable[i].free){
            fdTable[i].free = false;
            fdTable[i].iNodeNumber = iNodeNumber;
            fdTable[i].rwPointer = iNodeTable[iNodeNumber].size;
            return i;
        }
    }
    // If there is no empty space for a new file descriptor
    return -1;
}

int sfs_fopen(char* fileName){
    // check if the length is acceptable
    if(strlen(fileName) > MAXFILENAME){
        return -1;
    }
    
    // If the file exist but not opened
    for(int i = 0; i < NUMBER_OF_INODES; i ++){
        if(!directoryTable[i].free && strcmp(directoryTable[i].fileName, fileName) == 0){
            // check if the file is already opened
            for(int j = 0; j < NUMBER_OF_INODES; j ++){
                if(!fdTable[j].free && fdTable[j].iNodeNumber == directoryTable[i].iNodeNumber){
                    return -1;
                }
            }
            return addTofdTable(directoryTable[i].iNodeNumber);
        }
    }
    // If the file does not exist, then create a new one
    for(int i = 0; i < NUMBER_OF_INODES; i ++){
        if(directoryTable[i].free){
            // Create a new i-node
            for(int j = 0; j < NUMBER_OF_INODES; j ++){
                if(iNodeTable[j].free){
                    iNodeTable[j].free = false;
                    iNodeTable[j].size = 0;
                    iNodeTable[j].linkCount = 0;
                    iNodeTable[j].mode = 1;
                    // update the directory entry
                    directoryTable[i].free = false;
                    strcpy(directoryTable[i].fileName, fileName);
                    directoryTable[i].iNodeNumber = j;
                    flush(); // flush to the memory
                    return addTofdTable(directoryTable[i].iNodeNumber);
                }
            }
        }
    }
    // NO empty space, return -1
    return -1;
    
}

int sfs_fclose(int fileID){
    // Simply flag this cell in the file table is free, so we can reuse such cell
    if(!fdTable[fileID].free){
        fdTable[fileID].free = true;
        return 0;
    }
    return -1;
}


int getExtraBlocks(int fileID, int numOfBlocks) {
    //allocated blocks counter
    int count = 0;
    iNode node = iNodeTable[fdTable[fileID].iNodeNumber];

    for (int i = node.linkCount; i < node.linkCount + numOfBlocks; i++) {
        //check the bit map first
        for (int block = 0; block < NUMBER_OF_BLOCKS; block++) {
            if (bitMap[block] == 1) {
                bitMap[block] = 0;
                //check direct or indirect
                if (i < 12) {
                    node.pointer[i] = block;
                    count ++;
                } 
                else {
                    //if there isnt an indirect pointer already allocated, we allocate one
                    bool indirect = false;
                    if(node.linkCount + count == 12){
                        for (int indBlock = 0; indBlock < NUMBER_OF_BLOCKS; indBlock++) {
                            if (bitMap[indBlock] == 1) {
                                bitMap[indBlock] = 0;
                                node.indirectPointer = indBlock;
                                int indirectPointers[256];
                                // write to the corresponding block
                                write_blocks(node.indirectPointer, 1, indirectPointers);
                                indirect = true;
                                break;
                            }
                        }
                    }
                    else{
                        indirect = true;
                    }
                    //if no indirect pointer, we cannot make progress
                    if (indirect == false) break;
                    //we then add block to indirect pointers array
                    // get the blocks pointed by the indirect pointer
                    int indirectPointers[256];
                    read_blocks(node.indirectPointer, 1, indirectPointers);
                    indirectPointers[i-12] = block;
                    count ++;
                    // write to the correspondinng block
                    write_blocks(node.indirectPointer, 1, indirectPointers);
                }
                break;
            }
        }
    }
    // Update the information stored in the inode
    node.linkCount += count;
    iNodeTable[fdTable[fileID].iNodeNumber] = node;
    flush(); // flush to the disk
    return count;
}

int sfs_fwrite(int fileID, const char* buffer, int length){
    //check if file is open
    if(fdTable[fileID].free){
        return 0;
    }

    iNode node = iNodeTable[fdTable[fileID].iNodeNumber];
    //obtain the reading and writing pointer
    int rwPointer = fdTable[fileID].rwPointer;
    //where writing starts
    int StartBlock = rwPointer / BLOCK_SIZE;
    int StartByte = rwPointer % BLOCK_SIZE;
    //Exact number of bytes to be written
    int realLength = length;
    if (length + rwPointer > MAX_FILE_BLOCKS*BLOCK_SIZE){
        realLength = (MAX_FILE_BLOCKS * BLOCK_SIZE) - rwPointer;
        // realLength -= length + rwPointer - (MAX_FILE_BLOCKS * BLOCK_SIZE);
    }

    //calculating number of extra blocks needed
    int NumberOfExtraBlocks = 0;
    int temp = (rwPointer + realLength) / BLOCK_SIZE;
    if ((rwPointer + realLength) % BLOCK_SIZE != 0){
        temp++;
    }
    if(temp >= node.linkCount){
        NumberOfExtraBlocks = temp - node.linkCount;
    }
    //allocate extra blocks needed
    int actual = getExtraBlocks(fileID, NumberOfExtraBlocks);
    //update the information stored in the inode
    node = iNodeTable[fdTable[fileID].iNodeNumber];
    //if not enough blocks were allocated, update bytenum accordingly
    if(NumberOfExtraBlocks > actual){
        // update the real length if we cannnot write too long btyes
        NumberOfExtraBlocks = actual;
        realLength -= length + rwPointer - (node.linkCount * BLOCK_SIZE);
    }

    //initialize
    char aBuffer[(node.linkCount) * BLOCK_SIZE];
    memset(aBuffer, 0, sizeof(aBuffer));

    //Read whole file into aBuffer
    for (int i = 0; i < node.linkCount; i++ ){
        if(i < 12) {
            read_blocks(node.pointer[i], 1, aBuffer + (i * BLOCK_SIZE));
        }else{
            int indirectPointers[256];
            read_blocks(node.indirectPointer, 1, indirectPointers);
            for(int z = i; z < node.linkCount; z++){
                read_blocks(indirectPointers[z - 12], 1, aBuffer + (z * BLOCK_SIZE));
            }
            break;
        }
    }

    //write into buffer at right location via using StartBlock and StartByte
    memcpy(aBuffer+(StartBlock*BLOCK_SIZE)+StartByte, buffer, realLength);

    //Write the buffer back to the corresponding blocks
    for (int i = 0; i < node.linkCount; i++ ){
        if(i < 12){
            write_blocks(node.pointer[i], 1, aBuffer + (i*BLOCK_SIZE));
        }else{
            int indirectPointers[256];
            read_blocks(node.indirectPointer, 1, indirectPointers);

            for(int z = i; z < node.linkCount; z++){
                write_blocks(indirectPointers[z-12], 1, aBuffer + (z * BLOCK_SIZE));
            }
            break;
        }
    }

    //update file size and flush inode
    if(rwPointer + realLength > node.size){
        node.size = rwPointer + realLength;
    }
    iNodeTable[fdTable[fileID].iNodeNumber] = node;
    //update write pointer
    fdTable[fileID].rwPointer += realLength;
    //flush to the disk
    flush();
    return realLength;
}

int sfs_fread(int fileID, char* buffer, int length){
    // Check if the file is opened or not
    if(fdTable[fileID].free){
        return 0;
    }
    // Get the reading and writinng file pointer
    int index = fdTable[fileID].iNodeNumber;
    iNode node = iNodeTable[index];
    int rwPointer = fdTable[fileID].rwPointer;
    // Get the location of start block and the offset in the block
    int startBlock = rwPointer / BLOCK_SIZE;
    int startByte = rwPointer % BLOCK_SIZE;
    int realLength = length;
    if(rwPointer + length > node.size){
        realLength = node.size - rwPointer;
        // realLength -= rwPointer + length - node.size;
    }

    char readBuffer[node.linkCount * BLOCK_SIZE];
    memset(readBuffer, 0, sizeof(readBuffer));

    for(int i = 0; i < node.linkCount; i ++){
        if(i < 12){
            read_blocks(node.pointer[i], 1, readBuffer + (i * BLOCK_SIZE));
        }
        else{
            int indirectPointers[256];
            read_blocks(node.indirectPointer, 1, indirectPointers);

            for(int j = i; j < node.linkCount; j ++){
                read_blocks(indirectPointers[j - 12], 1, readBuffer + (j * BLOCK_SIZE));
            }
            break;
        }
    }

    memcpy(buffer, readBuffer + (startBlock * BLOCK_SIZE) + startByte, realLength);
    // Update the read and write pointer
    fdTable[fileID].rwPointer += realLength;
    // flush to the disk
    flush();
    return realLength;
}

// Update the location of the read and write pointer
int sfs_fseek(int fileID, int loc){
    if(loc >= 0 && loc <= iNodeTable[fdTable[fileID].iNodeNumber].size){
        fdTable[fileID].rwPointer = loc;
        return 0;
    }
    else{
        return -1;
    }
}

int sfs_remove(char* fileName){
    // Initialize a new inode
    iNode node;
    node.free = true;
    for(int i = 0; i < NUMBER_OF_INODES; i ++){
        if(!directoryTable[i].free){
            if(strcmp(directoryTable[i].fileName, fileName) == 0){
                for(int j = 0; j < NUMBER_OF_INODES; j ++){
                    if(!fdTable[j].free && fdTable[j].iNodeNumber == directoryTable[i].iNodeNumber){
                        return -1;
                    }
                }
                node = iNodeTable[directoryTable[i].iNodeNumber];
                iNodeTable[directoryTable[i].iNodeNumber].free = true;
                directoryTable[i].free = true;
                flush(); // flush to the disk
                break;
            }
        }
    }
    // No such file to remove
    if(node.free){
        return -1;
    }
    // Update the bitmap as well
    for(int i = 0; i < node.linkCount; i ++){
        if(i < 12){
            bitMap[node.pointer[i]] = 1;
        }
        else{
            bitMap[node.indirectPointer] = 1;
            int indirectPointers[256];
            read_blocks(node.indirectPointer, 1, indirectPointers);
            for(int j = i; j < node.linkCount; j ++){
                bitMap[indirectPointers[j - 12]] = 1;
            }
            break;
        }
    }
    flush();
    return 0;
}
