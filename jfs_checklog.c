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

    root_inode = find_root_directory(jfs);
    logfile_inode = findfile_recursive(jfs, ".log", root_inode, DT_FILE);
    if (logfile_inode < 0) {
		fprintf(stderr, "Missing logfile!\n");
    }else{
		get_inode(jfs, logfile_inode, &logfile_i_node);
		int i =0;
		while(logfile_i_node.blockptrs[i]){
			jfs_read_block(jfs,block,logfile_i_node.blockptrs[i]);
			commitblock = (struct commit_block*) block;
			//do stuff with block
			//printf("%s",block);
		}
		printf("\n");
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

