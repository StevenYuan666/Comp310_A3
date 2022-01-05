# Simple File System
To use this implemented simple file system, you need to download the files sfs_api.h and sfs_api.c

![image](https://user-images.githubusercontent.com/68981504/148151800-e85d7842-3c42-4052-b05f-2e3a37471fdf.png)

**mksfs(int flag)**

Formats the virtual disk implemented by the disk emulator and creates an instance of the simple file system on top of it. The mksfs() has a fresh flag to signal that the file system should be created from scratch. If flag is false, the file system is opened from the disk (i.e., we assume that a valid file system is already there in the file system. The support for persistence is important so you can reuse existing data or create a new file system.

**sfs_getnextfilename(char \*fname)**

Copies the name of the next file in the directory into fname and returns non zero if there is a new file. Once all the files have been returned, this function returns 0. So, you should be able to use this function to loop through the directory. 

**sfs_getfilesize(char \*path)**

Returns the size of a given file.

**sfs_fopen(char \*path)**

Opens a file and returns the index that corresponds to the newly opened file in the file descriptor table. If the file does not exist, it creates a new file and sets its size to 0. If the file exists, the file is opened in append mode (i.e., set the file pointer to the end of the file). 

**sfs_fclose(int filePointer)**

Closes a file, i.e., removes the entry from the open file descriptor table. On success, this function returns 0 and a negative value otherwise.

**sfs_fwrite(int filePointer, const char \*buffer, int size)**

Writes the given number of bytes of buffered data in buffer into the open file, starting from the current file pointer. This in effect could increase the size of the file by the given number of bytes (it may not increase the file size by the number of bytes written if the write pointer is located at a location other than the end of the file). This function returns the number of bytes written.

**sfs_fread(int filePointer, char \*buffer, int size)**

Reads the given number of bytes of buffered data in buffer into the open file, starting from the current file pointer. This function returns the number of bytes read.

**sfs_fseek(int filePointer, int position)**

Moves the read/write pointer (a single pointer in this Simple File System) to the given location. It return 0 on success and a negative integer value otherwise.

**sfs_remove(char \*fileName)**

Removes the file from the directory entry, releases the file allocation table entries and releases the data blocks used by the file, so that they can be used by new files in the future.
