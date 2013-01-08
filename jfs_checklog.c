#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

/* The following method takes the data present in the log file 
	writes them to the correct location in the disk
*/
void write_Data_To_Disk(jfs_t *jfs,int * blocks,struct inode logfile_i_node){
	int i =0;
	char writeBlock[BLOCKSIZE];
	//loop through the blocks in the log file
	while(blocks[i]!=-1 && i<INODE_BLOCK_PTRS){
		//reads the block in the log file
		jfs_read_block(jfs,writeBlock,logfile_i_node.blockptrs[i]);
		//writes to the diskk using the blocknumber present in the logfile commit block
		write_block(jfs->d_img,writeBlock,blocks[i]);
		i++;
	}
}
/*This is the main function for jfs_checklog command*/
void checklog(jfs_t *jfs)
{
    int root_inode,logfile_inode,i = 0,checksum =0;
	struct inode logfile_i_node;
	char block[BLOCKSIZE];
	struct commit_block *commitblock;
	//get the inode number of the root directory
    root_inode = find_root_directory(jfs);
	//use the findfile_recursive to find the inode number of the log file
    logfile_inode = findfile_recursive(jfs, ".log", root_inode, DT_FILE);
	//logfile is not found on the system so throw an error
    if (logfile_inode < 0) {
		fprintf(stderr, "Missing logfile!\n");
    }
	//otherwise continue on with the command
	else{
		//get the actual inode structure of the log file using the inode number found before
		get_inode(jfs, logfile_inode, &logfile_i_node);
		//loop through the block pointer in the inode
		while(logfile_i_node.blockptrs[i]){
			//add the block number to the checksum
			checksum+=blockptrs[i];
			//read in the block using the number from the block pointers array
			jfs_read_block(jfs,block,logfile_i_node.blockptrs[i]);
			//cast this block into a commit block
			commitblock = (struct commit_block *)block;
			//check if its the commit block by checking against the magic number
			if(commitblock->magicnum ==0x89abcdef){
				/*if the commit block has not been committed to disk and the checksum is equal to the one on the commit block
				*then call the write_Data_To_Disk function to write the data to disk
				*/
				if(commitblock->uncommitted && commitblock->sum == checksum){
					write_Data_To_Disk(jfs,commitblock->blocknums,logfile_i_node);
				}
				break;
			}
			i++;
		}
		
	}
    
}

void usage()
{
    fprintf(stderr, 
	    "Usage: jfs_checklog <volumename>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    struct disk_image *di;
    jfs_t *jfs;

    if (argc != 2) {
	usage();
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);

    checklog(jfs);

    unmount_disk_image(di);

    exit(0);
}

