
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
#define FREE_BLOCKS		   5
int MOUNTED = 0;
int BITMAP[FREE_BLOCKS]; 
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
	if (BLOCK.super.magic == FS_MAGIC) { 
		return 0;
	}

	int nblocks = disk_size();

	// read superblock information
	disk_read(0, BLOCK.data); 

	// set 10% of the blocks for inodes, needs to round up
	int ninodeblocks = 0.1 * nblocks;

	if (ninodeblocks == 0) {
		ninodeblocks = 1;
	}
	// BLOCK.super.nblocks=disk_size();
	printf("nblocks: %d\n", nblocks);

	// clear the inode table
	for(int j = 1; j <= ninodeblocks; j++) {
		disk_write(j, "0x0");
	}

	struct fs_superblock super;
	super.magic = FS_MAGIC;
	super.nblocks = nblocks;
	super.ninodeblocks = ninodeblocks; 
	super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	BLOCK.super = super;

	printf("%d\n", BLOCK.super.ninodeblocks);
	// writes the superblock
	// BLOCK.super.magic = FS_MAGIC;
	// BLOCK.super.nblocks = nblocks;
	// BLOCK.super.ninodeblocks = ninodeblocks;
	// BLOCK.super.ninodes = INODES_PER_BLOCK * ninodeblocks;

	disk_write(0, BLOCK.data);

	return 1;
}

void fs_debug()
{
	disk_read(0, BLOCK.data);

	printf("superblock:\n");
	printf("magic number: %d\n", BLOCK.super.magic);
	printf("    %d blocks\n",BLOCK.super.nblocks);
	printf("    %d inode blocks\n",BLOCK.super.ninodeblocks);
	printf("    %d inodes\n",BLOCK.super.ninodes);

}

int fs_mount()
{
	if (BLOCK.super.magic != FS_MAGIC) { 
		return 0;
	}

	return 1;
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
