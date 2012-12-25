#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

void checklog(jfs_t *jfs)
{
    int root_inode,logfile_inode;
	struct inode logfile_i_node;
	char block[BLOCKSIZE];
	struct commit_block *commitblock;
	unsigned int *magicnum;
	int bytes_done =0;

    root_inode = find_root_directory(jfs);
    logfile_inode = findfile_recursive(jfs, ".log", root_inode, DT_FILE);
    if (logfile_inode < 0) {
		fprintf(stderr, "Missing logfile!\n");
    }else{
		get_inode(jfs, logfile_inode, &logfile_i_node);
		jfs_read_block(jfs,block,logfile_i_node.blockptrs[0]);
		magicnum = (unsigned int*)block;
		while(1){
			if(*magicnum !=0x89abcdef){
				printf("commit block found\n");
				commitblock = (commit_block *)block;
				break;
			}else{
				magicnum = (unsigned int*)(block + sizeof(unsigned int));
				bytes_done+=sizeof(unsigned int);
				//do stuff with block
				//printf("%s",block);
				if(bytes_done>=BLOCKSIZE)
					break;
			}
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

