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
    struct dirent* dir_entry;
    char block[BLOCKSIZE];
    

    if (argc != 3) {
		usage();
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
	
	root_inode = find_root_directory(jfs);
	printf("root inode num: %d\n",root_inode);
	
	inode = findfile_recursive(jfs, argv[2], root_inode,DT_FILE);
	
	printf("inode num: %d\n",inode);
	
	get_inode(jfs, inode, &i_node);

	printf("File to remove: %s\n", argv[2]);
	
	printf("Inode: size = %d, flags = %d", i_node.size,i_node.flags);
	//read first block
	jfs_read_block(jfs,block,i_node.blockptrs[0]);
	//print block
	int j = 0;
		printf("%s",block);
	//return_inode_to_freelist(jfs,inode);
	int i =0;
	while(i_node.blockptrs[i]){
		printf(", block%d = %d ",i,i_node.blockptrs[i]);
		//return_block_to_freelist(jfs,i_node.blockptrs[i]);
		i++;
	}
	printf("\n");
    unmount_disk_image(di);

    exit(0);
}

