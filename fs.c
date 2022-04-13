
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
#define MEM_SIZE 		   4000000

int MOUNTED = 0;
int *bitmap; 
union fs_block BLOCK;
const char *EMPTY; 

int isFILESYS = 0;
int isMOUNTED = 0;

int FB_BITMAP[MEM_SIZE];

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
	// read superblock information
	disk_read(0, BLOCK.data); 
	
	// create a new filesystem on disk
	int nblocks = disk_size();
	int ninodeblocks = ( 0.1 * nblocks ) + 1; 
	// BLOCK.super.nblocks=disk_size();
	printf("nblocks: %d\n", nblocks);
	printf("ninodeblocks: %d\n", ninodeblocks);

	// invalidate all inode bits 
	for(int i = 1; i < ninodeblocks; i++) {
		union fs_block inode_block;
		for(int j = 0; j < INODES_PER_BLOCK; j++) {
			inode_block.inode[j].isvalid = 0;
		}
		disk_write(i, inode_block.data);
	}

	// destroy data already present
	/*for (int i = 0; i < DISK_BLOCK_SIZE; i++) {
		BLOCK.data[i] = 0;
	}*/
	// writing philo journal rn brb 你也去写 :))
	
	// clear the inode table
	/*for(int j = 1; j <= ninodeblocks; j++) {
		disk_write(j, EMPTY);
	}*/

	// writes the superblock
	BLOCK.super.magic = FS_MAGIC;
	BLOCK.super.nblocks = nblocks;
	BLOCK.super.ninodeblocks = ninodeblocks;
	BLOCK.super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	disk_write(0, BLOCK.data);

	isFILESYS = 1;
	return 1;


}

void fs_debug()
{
	disk_read(0, BLOCK.data);
	printf("superblock:\n");
	if(BLOCK.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	} else {
		printf("    magic number is invalid\n");
	}
	printf("    %d blocks on disk\n", BLOCK.super.nblocks);
	printf("    %d blocks for inodes\n", BLOCK.super.ninodeblocks);
	printf("    %d inodes total\n", BLOCK.super.ninodes);

	// check inode block
	for (int i = 1; i <= BLOCK.super.ninodeblocks; i++){
		disk_read(i, BLOCK.data);

		for (int j = 0; j < INODES_PER_BLOCK; j++){
			int inumber = (i-1) * INODES_PER_BLOCK + j;
			// get current inode
			struct fs_inode curr_inode = BLOCK.inode[j];
			printf("inode %d:\n", inumber);
			printf("\t size: %d bytes\n", curr_inode.size);

			// check for direct block

			// check for indirect block
		}
	}
}

int fs_mount()
{


	// check if disk is present
	if(!isFILESYS) {
		printf("no filesystem found\n");
		return 0;
	}

	// read superblock

	// build free block bitmap & prepare filesystem for use
	FB_BITMAP[0] = 1;
	int in;
	for(in = 1; in <= BLOCK.super.ninodeblocks; in++) {
		FB_BITMAP[in] = 1;
	}
	for(int ib = in+1; ib < BLOCK.super.nblocks; ib++) { 
		FB_BITMAP[ib] = 0;
	}

	isMOUNTED = 1;
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
