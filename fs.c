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
#define MEM_SET			   400000
#define MAX_OFFSET         4214784

int MOUNTED = 0;
int FORMATTED = 0;
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
	int ninodeblocks = 0.1 * nblocks;
	double check_num = 0.1 * (double) nblocks;

	if (ninodeblocks != check_num) {
		ninodeblocks += 1;
	}

	// clear the inode table, write null characters to every single block & byte
	for(int j = 1; j <= ninodeblocks; j++) {
		union fs_block curr_block;
		disk_read(j, curr_block.data);
		for (int k = 1; k <= INODES_PER_BLOCK; k++) {
			curr_block.inode[k].isvalid = 0;
			curr_block.inode[k].indirect = -1;
			int data_pointers[POINTERS_PER_BLOCK];
			for (int l = 0; l < POINTERS_PER_INODE; l++) {
				data_pointers[l] = -1;
				curr_block.inode[k].direct[l] = data_pointers[l];
			}
			if ((j == 2) && (k == 2)) {
				curr_block.inode[k].isvalid = 1;
				curr_block.inode[k].size = 67;
				curr_block.inode[k].direct[0] = 123;
				curr_block.inode[k].direct[1] = 134;
			}
		}
		disk_write(j, curr_block.data);
	}

	struct fs_superblock super;
	super.magic = FS_MAGIC;
	super.nblocks = nblocks;
	super.ninodeblocks = ninodeblocks; 
	super.ninodes = INODES_PER_BLOCK * ninodeblocks;
	SUPER_BLOCK.super = super;

	disk_write(0, SUPER_BLOCK.data);

	FORMATTED = 1;
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
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		// read the inode block for data
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
			int inumber = (i-1) * INODES_PER_BLOCK + j;
			if (curr_inode.isvalid == 1) {
				printf("inode %d:\n", inumber);
				printf("    size: %d bytes\n", curr_inode.size);
				if (curr_inode.size) {
					const char * space = " ";
					char direct_concat[MEM_SET];

					for (int k = 0; k < POINTERS_PER_INODE; k++) {
						if (curr_block.inode[j].direct[k] != -1) {
							char cstr[MEM_SET];
							sprintf(cstr, "%d", curr_block.inode[j].direct[k]);
							// concatenate direct blocks
							const char * src = cstr;
							if (k > 0) {
								strcat(direct_concat, space);
							}
							strcat(direct_concat, src);
						}
					}
					printf("    direct blocks: %s\n", direct_concat);
				}
			}
		}
	}
}

int fs_mount()
{
	if(!FORMATTED) {
		return 0;
	}
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

	MOUNTED = 1;
	return 1;
}

int fs_create()
{
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
	int inumber;
	// inode information
	for (int i = 1; i <= SUPER_BLOCK.super.ninodeblocks; i++) {
		// read the inode block for data
		union fs_block curr_block;
		disk_read(i, curr_block.data);
		for (int j = 1; j <= INODES_PER_BLOCK; j++) {
			struct fs_inode curr_inode = curr_block.inode[j];
			// create inode
			if (curr_inode.isvalid != 1) {
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
	if(!MOUNTED || !FORMATTED) {
		return 0;
	}
	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block inode_block;
	disk_read(block, inode_block.data);

	// fail if inode is invalid
	if(inode_block.inode[inode_in_block].isvalid != 1) {
		return 0;
	}

	inode_block.inode[inode_in_block].isvalid = 0;
	inode_block.inode[inode_in_block].size = 0;
	disk_write(block, inode_block.data);
	// update bitmap
	BITMAP[SUPER_BLOCK.super.ninodeblocks+inumber] = 0;
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
	if(inode_block.inode[inode_in_block].isvalid != 1) {
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

	// if string is empty, return 0
	if (length == 0) {
		return 0;
	}

	// if offset exceeds block size, return 0
	if (offset >= MAX_OFFSET) {
		return 0;
	}

	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block iblock;
	disk_read(block, iblock.data);

	// if the given inumber is invalid, return 0
	if(iblock.inode[inode_in_block].isvalid != 1) {
		return -1;
	}

	int ib;
	int curr_bit = -1;
	int blocks_needed;

	/* Prepare for writing */

	// Find free bit in bitmap
	for (ib = SUPER_BLOCK.super.ninodeblocks + 1; ib < SUPER_BLOCK.super.ninodeblocks; ib++) {
		if (BITMAP[ib] == 0) {
			curr_bit = ib;
			break;
		}
	}

	// no free bit, return 0
	if (curr_bit == -1) {
		return 0;
	}

	int curr_in_indirect;
	int offset_in_inode = offset / 4096;
	// direct block of inode
	if (offset_in_inode < 5) {
		// set pointer
		iblock.inode[inode_in_block].direct[offset_in_inode] = curr_bit;
	} else {
		union fs_block indirect_block;
		// if no indirect block allocated, allocate; else, read indirect block
		if (iblock.inode[inode_in_block].indirect != -1) {
			// make indirect block
			iblock.inode[inode_in_block].indirect = curr_bit;
			disk_read(curr_bit, indirect_block.data);

			int indirect_pointers[POINTERS_PER_BLOCK];

			for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
				indirect_pointers[i] = -1;
				indirect_block.pointers[i] = indirect_pointers[i];
			}

			disk_write(curr_bit, indirect_block.data);

			int used_bit = curr_bit;
			// Find free bit in bitmap
			for (ib = curr_bit + 1; ib < SUPER_BLOCK.super.ninodeblocks; ib++) {
				if (BITMAP[ib] == 0) {
					curr_bit = ib;
					break;
				}
			}

			// no free bit, return 0
			if (curr_bit == used_bit) {
				return 0;
			}

			// link the first direct block to indirect block
			indirect_block.pointers[0] = curr_bit;
		} else {
			disk_read(iblock.inode[inode_in_block].indirect, indirect_block.data);



			for (curr_in_indirect = 0; curr_in_indirect < POINTERS_PER_BLOCK; curr_in_indirect++) {
				if (indirect_block.pointers[curr_in_indirect] == -1) {
					break;
				}
			}

			// link the empty direct block to indirect block
			indirect_block.pointers[curr_in_indirect] = curr_bit;
		}
	}
	offset_in_inode += 1;

	offset = offset % 4096;
	union fs_block direct_block;
	disk_read(curr_bit, direct_block.data);

	char * r_data = data;

	int actual_written = 0;

	while (offset + length) {
		// check if lengtht + offset is less than a block size 
		if (length <= DISK_BLOCK_SIZE - offset) {
			char temp[length];
			snprintf(temp, length, "%s", r_data);
			memcpy(&direct_block.data[offset], temp, strlen(temp));

			// write to disk
			disk_write(curr_bit, direct_block.data);

			iblock.inode[inode_in_block].size += length;
			actual_written += length;
			// update size
			disk_write(block, iblock.data);

			offset = 0;
			length = 0;
		// length exceeds a block 
		} else {
			char temp[DISK_BLOCK_SIZE - offset];
			snprintf(temp, DISK_BLOCK_SIZE - offset, "%s", r_data);
			memcpy(&direct_block.data[offset], temp, strlen(temp));

			memcpy(r_data, &data[offset], length - DISK_BLOCK_SIZE + offset);
			// write to disk
			disk_write(curr_bit, direct_block.data);

			iblock.inode[inode_in_block].size += DISK_BLOCK_SIZE - offset;
			actual_written += DISK_BLOCK_SIZE - offset;
			// update size
			disk_write(block, iblock.data);

			// find new space for data block
			int used_bit = curr_bit;

			// Find free bit in bitmap
			for (ib = curr_bit + 1; ib < SUPER_BLOCK.super.ninodeblocks; ib++) {
				if (BITMAP[ib] == 0) {
					curr_bit = ib;
					break;
				}
			}

			// no free bit, return 0
			if (curr_bit == used_bit) {
				return actual_written;
			}

			if (offset_in_inode < 5) {
				// set pointer
				iblock.inode[inode_in_block].direct[offset_in_inode] = curr_bit;
			} else if (offset_in_inode == 5) {
				// have to allocate indirect block
				union fs_block indirect_block;
				// make indirect block
				iblock.inode[inode_in_block].indirect = curr_bit;
				disk_read(curr_bit, indirect_block.data);

				int indirect_pointers[POINTERS_PER_BLOCK];

				for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
					indirect_pointers[i] = -1;
					indirect_block.pointers[i] = indirect_pointers[i];
				}

				disk_write(curr_bit, indirect_block.data);

				int used_bit = curr_bit;
				// Find free bit in bitmap
				for (ib = curr_bit + 1; ib < SUPER_BLOCK.super.ninodeblocks; ib++) {
					if (BITMAP[ib] == 0) {
						curr_bit = ib;
						break;
					}
				}

				// no free bit, return 0
				if (curr_bit == used_bit) {
					return 0;
				}

				// link the first direct block to indirect block
				indirect_block.pointers[0] = curr_bit;
			} else {
				curr_in_indirect += 1;
				union fs_block indirect_block;
				disk_read(iblock.inode[inode_in_block].indirect, indirect_block.data);
				indirect_block.pointers
				[curr_in_indirect] = curr_bit;
			}
			offset_in_inode += 1;
			offset = 0;
			length = length - (DISK_BLOCK_SIZE - offset);
		}
	}
	return actual_written;
}