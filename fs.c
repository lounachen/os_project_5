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
#define MEM_SET			   4000

int MOUNTED = 0;
int BITMAP[MEM_SET]; 

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

union fs_block SUPER_BLOCK;

int fs_format() {

	// check if the file system has already been mounted → return fail if already mounted
	if (SUPER_BLOCK.super.magic == FS_MAGIC) { 
		return 0;
	}

	int nblocks = disk_size();

	// read superblock information
	disk_read(0, SUPER_BLOCK.data); 

	// set 10% of the blocks for inodes, needs to round up
	int ninodeblocks = 0.1 * nblocks + 1;

	// clear the inode table, write null characters to every single block & byte
	for(int j = 1; j <= ninodeblocks; j++) {
		union fs_block curr_block;
		disk_read(j, curr_block.data);
		for (int k = 1; k <= INODES_PER_BLOCK; k++) {
			//struct fs_inode curr_inode;
			curr_block.inode[k].isvalid = 0;
			// for testing
			if ((j == 1) && (k == 2)) {
				curr_block.inode[k].isvalid = 1;
				curr_block.inode[k].size = 52;
			}
			if ((j == 2) && (k == 2)) {
				curr_block.inode[k].isvalid = 1;
				curr_block.inode[k].size = 67;
			}
			disk_write(j, curr_block.data);
		}
	}

	struct fs_superblock super;
	super.magic = FS_MAGIC;
	super.nblocks = nblocks;
	super.ninodeblocks = ninodeblocks; 
	super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	SUPER_BLOCK.super = super;

	disk_write(0, SUPER_BLOCK.data);

	return 1;
}

void fs_debug()
{

	printf("superblock:\n");
	if(SUPER_BLOCK.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	} else {
		printf("    magic number is invalid\n");
	}
	printf("    %d blocks on disk\n",SUPER_BLOCK.super.nblocks);
	printf("    %d blocks for inodes\n",SUPER_BLOCK.super.ninodeblocks);
	printf("    %d inodes\n",SUPER_BLOCK.super.ninodes);

	// inode information
	for (int i = 1; i <= SUPER_BLOCK.super.ninodeblocks; i++) {
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		// read the inode block for data
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
			int inumber = (i-1) * INODES_PER_BLOCK + j;
			if (curr_inode.isvalid) {
				printf("inode %d:\n", inumber);
				printf("    size: %d bytes\n", curr_inode.size);
			}
		}
	}
}

int fs_mount()
{
	if (SUPER_BLOCK.super.magic != FS_MAGIC) { 
		return 0;
	}

	// build free block bitmap & prepare fs for use
	BITMAP[0] = 1;

	for (int in = 1; in <= SUPER_BLOCK.super.ninodeblocks; in++) {
		BITMAP[in] = 1;
	}

	for (int ib = SUPER_BLOCK.super.ninodeblocks + 1; ib < SUPER_BLOCK.super.ninodeblocks; ib++) {
		BITMAP[ib] = 0;
	}
	return 1;
}

int fs_create()
{
	int inumber;
	// inode information
	for (int i = 1; i <= SUPER_BLOCK.super.ninodeblocks; i++) {
		// read the inode block for data
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
			printf("validity of inode %d: %d\n", j, curr_inode.isvalid);
			// create inode
			if (!curr_inode.isvalid) {
				inumber = (i-1) * INODES_PER_BLOCK + j;
				struct fs_inode inode;
				inode.isvalid = 1;
				inode.size = 0;
				curr_block.inode[j] = inode;
				disk_write(i, curr_block.data);
				return inumber;
			}
		}
	}
	return 0;
}

int fs_delete( int inumber )
{
	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block inode_block;
	disk_read(block, inode_block.data);

	// fail if inode is invalid
	if(!inode_block.inode[inode_in_block].isvalid) {
		return 0;
	}

	inode_block.inode[inode_in_block].isvalid = 0;
	inode_block.inode[inode_in_block].size = 1;
	return 1;
}

int fs_getsize( int inumber )
{
	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block inode_block;
	disk_read(block, inode_block.data);
	
	// fail if inode is invalid
	if(!inode_block.inode[inode_in_block].isvalid) {
		return -1;
	}

	int size = inode_block.inode[inode_in_block].size;
	return size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
