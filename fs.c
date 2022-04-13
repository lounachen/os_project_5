
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

int MOUNTED = 0;
int *bitmap; 
union fs_block BLOCK;
const char *EMPTY; 

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format() {
	// check if the file system has already been mounted → return fail if already mounted
	if (BLOCK.super.magic == FS_MAGIC) {  // Hanjing: I think we need to have this function in mount, not format why
		return 0;  							// bc "format rountine places this FS_MAGIC into superblock" 
	}										// "When the filesystem is mounted, the OS looks for this magic number."
											// so the comparison of magic to FS_MAGIC is in mount
											// yea so like when they are equal its mounted? if not it isnt 很有道理！
	
	// read superblock information
	disk_read(0, BLOCK.data); 
	
	// create a new filesystem on disk
	int nblocks = disk_size();
	int ninodeblocks = 0.1 * nblocks; 
	// BLOCK.super.nblocks=disk_size();
	printf("nblocks: %d\n", nblocks);

	// destroy data already present
	/*for (int i = 0; i < DISK_BLOCK_SIZE; i++) {
		BLOCK.data[i] = 0;
	}*/
	// writing philo journal rn brb 你也去写 :))

	// set 10% of the blocks for inodes, needs to round up
	// BLOCK.inode.size = 0.1 * nblocks; // what is exactly this 10% thing for? 
	
	// clear the inode table
	for(int j = 1; j <= ninodeblocks; j++) {
		disk_write(j, EMPTY);
	}

	// writes the superblock
	BLOCK.super.magic = FS_MAGIC;
	BLOCK.super.nblocks = nblocks;
	BLOCK.super.ninodeblocks = ninodeblocks;
	BLOCK.super.ninodes = INODE_PER_BLOCK * ninodeblocks;
	disk_write(0, BLOCK.data);
	return 1;


}

void fs_debug()
{
	union fs_block block;

	disk_read(0,block.data);

	printf("superblock:\n");
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);
}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
