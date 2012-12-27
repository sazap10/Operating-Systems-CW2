#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

int main(int argc, char **argv){

	struct disk_image *di;
    jfs_t *jfs;
	int root_inode;
	struct inode i_node;
	int dir_size;
	int file_inodenum;
	struct dirent *dir_entry; 
	char firstpart[MAX_FILENAME_LEN];
	char block[BLOCKSIZE];
	int block_num;
	int bytes_done=0;
	
    	/* mount the disk image  file so we can access it*/
    	di = mount_disk_image(argv[1]);

    	jfs = init_jfs(di);

	//get the inode of the directory entry
	root_inode = find_root_directory(jfs);

	get_inode(jfs,root_inode, &i_node);
  	dir_size = i_node.size;

	char *pathname = argv[2];

	while(pathname[0]=='/') {
		pathname++;
   	}
	
  	file_inodenum = findfile_recursive(jfs, pathname, root_inode, DT_FILE);
	printf("This is the file i node number: %d\n", file_inodenum);

    	if (file_inodenum < 0) {
		fprintf(stderr, "Cannot open file %s\n", pathname);
		exit(1);
    	}	
	
	// read the directory entry
	block_num = inode_to_block(file_inodenum); 
	jfs_read_block(jfs, block, block_num);
	printf("Read the directory!\n");

	//find the directory of the inode and remove that directory entry
	dir_entry = (struct dirent*)block;
				
	printf("Has assigned the directory\n");
	
	//then find the blocks the inode is linked to and remove the links	
	return_inode_to_freelist(jfs, file_inodenum);
	
	while (1) {	
		char filename[MAX_FILENAME_LEN + 1];
		memcpy(filename, dir_entry->name, dir_entry->namelen);
		filename[dir_entry->namelen] = '\0'; //segmentation fault below
		printf("%s\n",filename);

		if (!strcmp(firstpart, filename)) {
			printf("Found the file");
			//create a new block write everything before the dirent into the new block and everything after it, 
			//then write it to the file system
			
			char newblock[BLOCKSIZE];
			memcpy(newblock, dir_entry->name, dir_entry->namelen);
			jfs_write_block(jfs, newblock, block_num);
			break;
		}
		else{ 
			bytes_done += dir_entry->entry_len;
			dir_entry = (struct dirent*)(block + bytes_done);

			if (bytes_done >= dir_size) {
	 			break;
			}
		}
	}

	return 1;
}

