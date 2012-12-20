#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

void usage()
{
    fprintf(stderr, 
	    "Usage: jfs_rm <volumename> <filename>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    struct disk_image *di;
    jfs_t *jfs;
	struct inode i_node;
	int root_inode;
	int inode;
    

    if (argc != 3) {
		usage();
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
	
	root_inode = find_root_directory(jfs);
	printf("root inode num: %d",root_inode);
	
	inode = findfile_recursive(jfs, args[2], root_inode,DT_DIRECTORY);
	
	printf("inode num: %d",inode);
	
	//get_inode(jfs, root_inode, &i_node);


	printf("File to remove: %s\n", argv[2]);
	

    unmount_disk_image(di);

    exit(0);
}

