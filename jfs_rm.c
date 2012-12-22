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
	
	//inode = findfile_recursive(jfs, argv[2], root_inode,DT_FILE);
	
	//printf("inode num: %d\n",inode);
	
	get_inode(jfs, root_inode, &i_node);

	printf("File to remove: %s\n", argv[2]);
	
	printf("Inode to block: %d\n",inode_to_block(inode));
	
	jfs_read_block(jfs, block, i_node.blockptrs[0]);
	
	printf("%s\n",block);
	/*
	dir_entry = (struct dirent*)block;
	char filename[MAX_FILENAME_LEN + 1];
	memcpy(filename, dir_entry->name, dir_entry->namelen);
	filename[dir_entry->namelen] = '\0';
	printf("%s\n",filename);
	//printf("dir_entry: file_type=%d entry_len=%d inode=%d namelen=%d name=%s\n",dir_entry->file_type,dir_entry->entry_len,dir_entry->inode,dir_entry->namelen,filename);

	//printf("Inode: size = %d, flags = %d\n", i_node.size,i_node.flags);
	
	//set the inode as free	
	//return_inode_to_freelist(jfs,inode);
	/*used to loop through the blocks of the file
	int i =0;
	
	while(i_node.blockptrs[i]){
		//printf(", block%d = %d ",i,i_node.blockptrs[i]);
		jfs_read_block(jfs,block,i_node.blockptrs[i]);
		printf("%s",block);
		//set block as free
		//return_block_to_freelist(jfs,i_node.blockptrs[i]);
		i++;
	}*/
	printf("\n");
    unmount_disk_image(di);

    exit(0);
}

