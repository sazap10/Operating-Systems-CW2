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

void checklog(jfs_t *jfs)
{
    int root_inode,logfile_inode;
	struct inode logfile_i_node;
	char block[BLOCKSIZE];
	struct commit_block *commitblock;
	int i = 0;
	
    root_inode = find_root_directory(jfs);
    logfile_inode = findfile_recursive(jfs, ".log", root_inode, DT_FILE);
    if (logfile_inode < 0) {
		fprintf(stderr, "Missing logfile!\n");
    }else{
		get_inode(jfs, logfile_inode, &logfile_i_node);
		while(logfile_i_node.blockptrs[i]){
			jfs_read_block(jfs,block,logfile_i_node.blockptrs[i]);
			commitblock = (struct commit_block *)block;
			if(commitblock->magicnum ==0x89abcdef){
				//printf("commit block found\n");
				if(commitblock->uncommitted){
					write_Data_To_Disk(jfs,commitblock->blocknums,logfile_i_node);
				}
				memset(block,0,BLOCKSIZE);
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

