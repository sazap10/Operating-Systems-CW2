#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"


void jfs_checklog(jfs_t *jfs)
{
	struct commit_block* commitBlock;
	struct inode fileInode;
	int rootInode = find_root_directory(jfs);
	int logFileInode = findfile_recursive(jfs,".log", rootInode, DT_FILE);
	
	if(logFileInode == -1)
	{
		printf("Logfile does not exist.");
		return;
	}

	char block[BLOCKSIZE];
	char copyBlock[BLOCKSIZE];
	get_inode(jfs,logFileInode, &fileInode) ;
	int n = 0;
	for(n=0;n<INODE_BLOCK_PTRS;n++)
	{
		if(fileInode.blockptrs[n])
		{
		
				jfs_read_block(jfs, block, fileInode.blockptrs[n]);  
				commitBlock = (struct commit_block*) block;
				if(commitBlock->magicnum == 0x89abcdef)
				{
					if(commitBlock->uncommitted)
					{
						int m = 0;
						for(m=0;m<n;m++)
						{	
							jfs_read_block(jfs, copyBlock, fileInode.blockptrs[m]); 
							write_block(jfs->d_img, copyBlock, commitBlock->blocknums[m]);
						}
					}
					
				}
		}
	}
}



void usage()
{
  fprintf(stderr, "Usage: jfs_checklog <volumename>\n");
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
    jfs_checklog(jfs);

    unmount_disk_image(di);
    
    exit(0);
}

