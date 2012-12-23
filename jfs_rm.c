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

void jfs_remove_file(jfs_t *jfs,char *filename){
	struct inode file_i_node;
	struct inode dir_i_node;
	int root_inode;
	int file_inode;
	int dir_inode;
    struct dirent* dir_entry;
    char block[BLOCKSIZE];
	char rest[MAX_FILENAME_LEN];
	
	root_inode = find_root_directory(jfs);
	printf("root inode num: %d\n",root_inode);
	
	all_but_last_part(filename,rest);
	/*
	first_part(argv[2],firstpart);
	last_part(argv[2],lastpart);
	all_but_last_part(argv[2],rest);
	
	printf("%s\n",firstpart);
	printf("%s\n",lastpart);
	printf("%s\n",rest);
	*/
	
	if(file_inode = findfile_recursive(jfs, filename, root_inode,DT_FILE)==-1){
		printf("rm: cannot remove '%s': No such file or directory",filename);
		return;
	}
	//printf("inode num: %d\n",inode);
	
	get_inode(jfs, file_inode, &file_i_node);
	
	get_inode(jfs,dir_inode,&dir_i_node);

	printf("File to remove: %s\n", filename);
	
	jfs_read_block(jfs, block, dir_i_node.blockptrs[0]);
	
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
}

int main(int argc, char **argv)
{
    struct disk_image *di;
    jfs_t *jfs;
    
    if (argc != 3) {
		usage();
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
	
	jfs_remove_file(jfs,argv[2]);
	
    unmount_disk_image(di);

    exit(0);
}

