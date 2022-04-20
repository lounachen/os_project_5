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

void inode_load( int inumber, struct fs_inode *inode ) {
	int block_number = inumber / INODES_PER_BLOCK + 1;
	int offset = inumber % INODES_PER_BLOCK;

	union fs_block block;
	disk_read(block_number, block.data);
	// load inode
	*inode = block.inode[offset];
	return;
}

void inode_save( int inumber, struct fs_inode *inode ) {
	int block_number = inumber / INODES_PER_BLOCK + 1;
	int offset = inumber % INODES_PER_BLOCK;

	union fs_block block;
	disk_read(block_number, block.data);
	// save inode
	block.inode[offset] = *inode;
	// write back
	disk_write(block_number, block.data);
	return;
}

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

	// clear the inode table, write null characters (0) to every single block & byte
	for(int j = 1; j <= ninodeblocks; j++) {
		union fs_block curr_block;
		disk_read(j, curr_block.data);
		for (int k = 1; k <= INODES_PER_BLOCK; k++) {
			curr_block.inode[k].isvalid = 0;
			curr_block.inode[k].indirect = 0;
			int data_pointers[POINTERS_PER_BLOCK];
			for (int l = 0; l < POINTERS_PER_INODE; l++) {
				data_pointers[l] = 0;
				curr_block.inode[k].direct[l] = data_pointers[l];
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
			int inumber = (i-1) * INODES_PER_BLOCK + j; // calculate inode # based on placement in block
			if (curr_inode.isvalid == 1) {
				printf("inode %d:\n", inumber);
				printf("    size: %d bytes\n", curr_inode.size);
				if (curr_inode.size) {
					printf("    direct blocks: ");
					for (int k = 0; k < POINTERS_PER_INODE; k++) {
						if (curr_block.inode[j].direct[k] != 0) {
							if (k > 0) {
								printf(" ");
							}

							printf("%d", curr_block.inode[j].direct[k]);
						}
					}
					printf("\n");
					if(curr_block.inode[j].indirect != 0) {
						printf("    indirect block: %d\n", curr_block.inode[j].indirect);
						union fs_block indirect_block;
						disk_read(curr_block.inode[j].indirect, indirect_block.data);
						printf("    indirect data blocks: ");
						for (int data = 0; data < POINTERS_PER_INODE; data++) {
							if(indirect_block.pointers[data]) {
								printf(" %d", indirect_block.pointers[data]);
							}
						}
						printf("\n");
					}
				}
			}
		}
	}
	/*
	for(int i = 0; i < SUPER_BLOCK.super.nblocks; i++) {
		printf("%d ", BITMAP[i]);
	}
	printf("\n");*/
}

// build free block bitmap & prepare fs for use
int fs_mount()
{
	disk_read(0, SUPER_BLOCK.data); 
	//printf("ninodeblocks: %d\n", SUPER_BLOCK.super.ninodeblocks);
	if (SUPER_BLOCK.super.magic != FS_MAGIC) { 
		return 0;
	}

	// set superblock to 1 in bitmap
	BITMAP[0] = 1;

	// clear data bitmap
	for (int ib = SUPER_BLOCK.super.ninodeblocks + 1; ib < SUPER_BLOCK.super.ninodes; ib++) {
		BITMAP[ib] = 0;
	}

	for (int in = 1; in <= SUPER_BLOCK.super.ninodeblocks; in++) {
		union fs_block inode_block;
		disk_read(in, inode_block.data);
		// check inodes for existing data & update bitmap accordingly
		for(int ifdata = 0; ifdata < INODES_PER_BLOCK; ifdata++) {
			if(inode_block.inode[ifdata].isvalid) {
				// check for direct data
				for (int data = 0; data < POINTERS_PER_INODE; data++) {
					if (inode_block.inode[ifdata].direct[data] > 0) {
						BITMAP[inode_block.inode[ifdata].direct[data]] = 1;
					} 
				}
				// check for indirect data
				if((inode_block.inode[ifdata].indirect > 0) ) {
					BITMAP[inode_block.inode[ifdata].indirect] = 1;
					for(int data = 0; data < POINTERS_PER_INODE; data++) {
						if (inode_block.inode[ifdata].direct[data] != 0) {
							BITMAP[inode_block.inode[ifdata].direct[data]] = 1;
						}
					}
				}
			}
		}
		BITMAP[in] = 1; // set inode block to 1 in bitmap
	} 

	MOUNTED = 1; // boolean necessary for other functions
	return 1;
}

int fs_create()
{
	if(!MOUNTED) {
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
	if(!MOUNTED) {
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
	int curr_bit;
	//invalidate & clear direct blocks associated with inode
	for (int offset_in_inode=0; offset_in_inode<5; offset_in_inode++){
		if (inode_block.inode[inode_in_block].direct[offset_in_inode]!=0){
			curr_bit=inode_block.inode[inode_in_block].direct[offset_in_inode];
			inode_block.inode[inode_in_block].direct[offset_in_inode]=0;
			BITMAP[curr_bit]=0;
		}
	}
	//clear indirect block
	if(inode_block.inode[inode_in_block].indirect!=0){
		curr_bit=inode_block.inode[inode_in_block].indirect;
		inode_block.inode[inode_in_block].indirect=0;
		BITMAP[curr_bit]=0;
	}
	// invalidate & clear inode
	inode_block.inode[inode_in_block].isvalid = 0;
	inode_block.inode[inode_in_block].size = 0;
	disk_write(block, inode_block.data);
	// update bitmap
	BITMAP[SUPER_BLOCK.super.ninodeblocks+inumber] = 0;
	return 1;
}

int fs_getsize( int inumber )
{
	if(!MOUNTED) {
		return 0;
	}

	struct fs_inode curr_inode;
	inode_load(inumber, &curr_inode);
	
	// fail if inode is invalid
	if(!curr_inode.isvalid) {
		return -1;
	}

	int size = curr_inode.size;
	return size;
}

int fs_read( int inumber, char *data, int length, int offset )
{

	if(!MOUNTED) {
		return 0;
	}

	struct fs_inode curr_inode;
	inode_load(inumber, &curr_inode);

	if (length == 0) {
		fprintf(stderr, "Error: Read 0 byte\n");
		return 0;
	}

	// if the given inumber is invalid, return 0
	if (curr_inode.isvalid == 0) {
		fprintf(stderr, "Error: Inode %d is not valid\n", inumber);
		return 0;
	}

	if (offset > curr_inode.size) {
		fprintf(stderr, "Error: Offset is too large\n");
		return 0;
	}

	// check if bytes read will exceed inode's size
	// if yes, adjust the length
	if (offset + length > curr_inode.size) {
		length = curr_inode.size - offset;
	}

	int actual_read = 0;
	int block_in_inode = offset / DISK_BLOCK_SIZE; // same with offset_in_inode in fs_write
	int offset_in_block = offset % DISK_BLOCK_SIZE;

	while (block_in_inode < POINTERS_PER_INODE && length) {
		union fs_block block;
		disk_read(curr_inode.direct[block_in_inode], block.data);

		// calculate the size of read
		// cut short if the bytes going to read exceed the block size
		int read_byte = (offset_in_block + length > DISK_BLOCK_SIZE)
						?(DISK_BLOCK_SIZE - offset_in_block) 
						: (length - offset_in_block);
		
		// copy the bytes from inode to the data pointer
		memcpy(data+read_byte, block.data+offset_in_block, read_byte);
		actual_read += read_byte;
		length -= read_byte;
		offset_in_block = 0;
		block_in_inode++;
	}

	// if all we used is direct pointers
	if (!length) return actual_read;

	// if we need to read indirect pointer
	union fs_block indirect_pointer;
	disk_read(curr_inode.indirect, indirect_pointer.data);
	block_in_inode -= POINTERS_PER_INODE;

	// read from indirect block's pointer
	while (length) {
		union fs_block block;
		disk_read(indirect_pointer.pointers[block_in_inode], block.data);
		int read_byte = (offset_in_block + length > DISK_BLOCK_SIZE)
						?(DISK_BLOCK_SIZE - offset_in_block) 
						: (length - offset_in_block);
		// copy the bytes from inode to the data pointer
		memcpy(data+read_byte, block.data+offset_in_block, read_byte);
		
		actual_read += read_byte;
		length -= read_byte;
		offset_in_block = 0;
		block_in_inode++;
	}

	return actual_read;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	
	
	if(!MOUNTED) {
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

	// if offset exceeds block size, return -1
	if (offset >= MAX_OFFSET) {
		return -1;
	}

	int inode_in_block = inumber % INODES_PER_BLOCK;
	int block = (inumber - inode_in_block) / INODES_PER_BLOCK + 1;

	union fs_block iblock;
	disk_read(block, iblock.data);

	// if the given inumber is invalid, return -1
	if(iblock.inode[inode_in_block].isvalid != 1) {
		printf("ERROR: inode not created\n");
		return -1;
	}

	int bit;
	int curr_bit = -1;
	//int blocks_needed;

	/* Prepare for writing */

	// Find free bit in bitmap
	for (bit = SUPER_BLOCK.super.ninodeblocks + 1; bit < SUPER_BLOCK.super.nblocks; bit++) {
		if (BITMAP[bit] == 0) {
			curr_bit = bit;
			BITMAP[bit] = 1;
			break;
		}
	}

	// no free bit, return 0
	if (curr_bit == -1) {
		printf("ERROR: no free blocks available\n");
		return -1;
	}

	int curr_in_indirect;

	// direct[0|1|2|3|4] or indirect
	int offset_in_inode = offset / DISK_BLOCK_SIZE;
	// one of the direct blocks of inode
	if (offset_in_inode < POINTERS_PER_INODE) {
		
		// set pointer
		if (iblock.inode[inode_in_block].direct[offset_in_inode] != curr_bit) {
			iblock.inode[inode_in_block].direct[offset_in_inode] = curr_bit;
			BITMAP[curr_bit] = 1;
		}
	} else { // indirect block
		union fs_block indirect_block;
		// if no indirect block allocated, allocate; else, read indirect block
		if (iblock.inode[inode_in_block].indirect == 0) {
			// make indirect block
			iblock.inode[inode_in_block].indirect = curr_bit;
			disk_read(curr_bit, indirect_block.data);

			int indirect_pointers[POINTERS_PER_BLOCK];

			// initialize all pointers of the indirect block to 0
			for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
				indirect_pointers[i] = 0;
				indirect_block.pointers[i] = indirect_pointers[i];
			}

			int used_bit = curr_bit;

			// Find free bit in bitmap
			for (bit = curr_bit + 1; bit < SUPER_BLOCK.super.nblocks; bit++) {
				if (BITMAP[bit] == 0) {
					BITMAP[used_bit] = 1;
					curr_bit = bit;
					break;
				}
			}

			// no free bit, return 0
			if (curr_bit == used_bit) {
				return 0;
			}
			iblock.inode[inode_in_block].size += DISK_BLOCK_SIZE;

			disk_write(used_bit, indirect_block.data);

			// link the first direct block to indirect block
			indirect_block.pointers[0] = curr_bit;
		} else { // indirect block exists, access it

			disk_read(iblock.inode[inode_in_block].indirect, indirect_block.data);

			// find vacancy in indirect block

			for (curr_in_indirect = 0; curr_in_indirect < POINTERS_PER_BLOCK; curr_in_indirect++) {
				if (indirect_block.pointers[curr_in_indirect] == 0) {
					break;
				}
			}

			// link the empty direct block to indirect block
			indirect_block.pointers[curr_in_indirect] = curr_bit;
		}
	}
	//offset_in_inode +=1;
	offset = offset % 4096;

	union fs_block direct_block;
	disk_read(curr_bit, direct_block.data);

	char r_data[strlen(data)+1];
    strncpy(r_data, data, strlen(data)+1); // i think this poses an issue

	int actual_written = 0;

	while (offset + length) {
		// check if length + offset is less than a block size 
		if (length <= DISK_BLOCK_SIZE - offset) {
			char temp[length];
			snprintf(temp, length, "%s", data); //changed from r_data
			memcpy(&direct_block.data[offset], temp, strlen(temp));
			
			if (offset_in_inode < 5){
				curr_bit+=1;
				iblock.inode[inode_in_block].direct[offset_in_inode] = curr_bit;
			}
			// write to disk
			disk_write(curr_bit, direct_block.data);
			iblock.inode[inode_in_block].size += length;
			actual_written += length;
			// update size
			disk_write(block, iblock.data);

			offset = 0;
			length = 0;
			
			return actual_written;

		// length exceeds a block 
		} else {
			char temp[DISK_BLOCK_SIZE - offset];
			snprintf(temp, DISK_BLOCK_SIZE - offset, "%s", r_data); //puts into temp as much data as we can handle for now
			memcpy(&direct_block.data[offset], temp, strlen(temp)); //what is direct block refering to here?

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
			for (bit = curr_bit + 1; bit < SUPER_BLOCK.super.nblocks; bit++) {
				if (BITMAP[bit] == 0) {
					curr_bit = bit;
					BITMAP[bit] = 1;
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
					indirect_pointers[i] = 0;
					indirect_block.pointers[i] = indirect_pointers[i];
				}

				disk_write(curr_bit, indirect_block.data);

				int used_bit = curr_bit;
				// Find free bit in bitmap
				for (bit = curr_bit + 1; bit < SUPER_BLOCK.super.nblocks; bit++) {
					if (BITMAP[bit] == 0) {
						curr_bit = bit;
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
				indirect_block.pointers[curr_in_indirect] = curr_bit;
			}
			offset_in_inode += 1;
			offset = 0;
			length = length - (DISK_BLOCK_SIZE - offset);
		}
	}
	return actual_written;
}
