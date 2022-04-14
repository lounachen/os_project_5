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
<<<<<<< HEAD
=======
int FORMATTED = 0;
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
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
<<<<<<< HEAD
union fs_block INODE_BLOCKS[MEM_SET];


=======
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64

int fs_format() {

	// check if the file system has already been mounted → return fail if already mounted
	if (SUPER_BLOCK.super.magic == FS_MAGIC) { 
		return 0;
	}

	int nblocks = disk_size();

	// read superblock information
	disk_read(0, SUPER_BLOCK.data); 

	// set 10% of the blocks for inodes, needs to round up
	int ninodeblocks = 0.1 * nblocks;
	double check_num = 0.1 * (double) nblocks;

	if (ninodeblocks != check_num) {
		ninodeblocks += 1;
	}

<<<<<<< HEAD
	// clear the inode table
	for(int j = 1; j <= ninodeblocks; j++) {
		for (int k = 1; k <= INODES_PER_BLOCK; k++) {
			struct fs_inode inode;
			inode.isvalid = 0;
			// for testing
			if ((j == 1) && (k == 2)) {
				inode.isvalid = 1;
				inode.size = 52;
			}
			if ((j == 2) && (k == 2)) {
				inode.isvalid = 1;
				inode.size = 67;
			}
			INODE_BLOCKS[j].inode[k] = inode;
=======
	// clear the inode table, write null characters to every single block & byte
	for(int j = 1; j <= ninodeblocks; j++) {
		union fs_block curr_block;
		disk_read(j, curr_block.data);
		for (int k = 1; k <= INODES_PER_BLOCK; k++) {
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
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
		}
	}

	struct fs_superblock super;
	super.magic = FS_MAGIC;
	super.nblocks = nblocks;
	super.ninodeblocks = ninodeblocks; 
	super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	SUPER_BLOCK.super = super;

	disk_write(0, SUPER_BLOCK.data);

<<<<<<< HEAD
=======
	FORMATTED = 1;
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
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
	printf("    %d inodes total\n",SUPER_BLOCK.super.ninodes);

	// inode information
	for (int i = 1; i <= SUPER_BLOCK.super.ninodeblocks; i++) {
<<<<<<< HEAD

		// read the inode block for data
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = INODE_BLOCKS[i].inode[j];
=======
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		// read the inode block for data
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
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
<<<<<<< HEAD
=======
	if(!FORMATTED) {
		return 0;
	}
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
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
<<<<<<< HEAD
=======

	MOUNTED = 1;
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
	return 1;
}

int fs_create()
{
<<<<<<< HEAD
=======
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
	int inumber;
	// inode information
	for (int i = 1; i <= SUPER_BLOCK.super.ninodeblocks; i++) {
		// read the inode block for data
<<<<<<< HEAD
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = INODE_BLOCKS[i].inode[j];
=======
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
			// create inode
			if (!curr_inode.isvalid) {
				inumber = (i-1) * INODES_PER_BLOCK + j;
				struct fs_inode inode;
				inode.isvalid = 1;
				inode.size = 0;
<<<<<<< HEAD
				INODE_BLOCKS[i].inode[j] = inode;

=======
				curr_block.inode[j] = inode;
				disk_write(i, curr_block.data);
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
				return inumber;
			}
		}
	}
	return 0;
}

int fs_delete( int inumber )
{
<<<<<<< HEAD
	int j = inumber % INODES_PER_BLOCK;
	int i = (inumber - j) / INODES_PER_BLOCK + 1;

	// on failure, return 0
	if (!INODE_BLOCKS[i].inode[j].isvalid) {
		return 0;
	}

	struct fs_inode inode;
	inode.isvalid = 0;
	inode.size = 1;
	INODE_BLOCKS[i].inode[j] = inode;
=======
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
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
>>>>>>> aecb7a75e1836db40a4e44686fe7d9ea620cba64
	return 1;
}

int fs_getsize( int inumber )
{
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
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
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}

	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{

	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
	/* Write data to a valid inode. 
	Allocate any necessary direct and indirect blocks in the process. 
	Return the number of bytes actually written. 
	The number of bytes actually written could be smaller 
	than the number of bytes request, perhaps if the disk becomes full. 
	If the given inumber is invalid, or any other error is encountered, return 0.*/


	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block iblock;
	disk_read(block, iblock.data);

	// if the given inumber is invalid, return 0
	if(!iblock.inode[inode_in_block].isvalid) {
		return -1;
	}

	// check if length is less than a block size 



	return 0;
}