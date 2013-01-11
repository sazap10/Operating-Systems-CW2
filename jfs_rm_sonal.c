#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"


/*
get the inode of the directory entry
read that directory entry
then find the blocks the inode is linked to and remove the links
find the directory of the inode and remove that directory entry
update the size of the directory
do not use system calls....i.e. unlink, and link
*/

int main(int argc, char **argv){

	struct disk_image *di;
    	jfs_t *jfs;
	
	if (argc != 3) {
		fprintf(stderr, 
	  	"Usage: jfs_rm <volumename> <filename>\n");
  	  	exit(1);
	}

	/* mount the disk image  file so we can access it*/
    	di = mount_disk_image(argv[1]);
    	jfs = init_jfs(di);

	struct dirent *dir_entry;
	struct inode i_node, dir_inode; //inode for directory and file
	int root_inodenum, file_inodenum, dir_inodenum, dir_size, bytes_done=0;
	char file[MAX_FILENAME_LEN], dirName[MAX_FILENAME_LEN]; //file refers to the actual file name
	char block[BLOCKSIZE];
	int block_num;
	char *pathname = argv[2];
	char newblock[BLOCKSIZE];
	char *file_before_rm, *file_after_rm;

	/*
	get the inode of the root directory so that 
	you can search for the file and directory
	then get the path of the directory where the file has been kept
	also get the files name 	
	*/
	
	root_inodenum = find_root_directory(jfs);
	all_but_last_part(pathname,dirName);
	last_part(pathname, file);

	while(pathname[0]=='/') {
		pathname++;
   	}
	
	/*
	we now need to get the i node of the file, 
	however we need to make sure that the file actually exists. 
	*/
  	file_inodenum = findfile_recursive(jfs, pathname, root_inodenum, DT_FILE);
	printf("This is the file i node number: %d\n", file_inodenum);

    	if (file_inodenum < 0) {
		fprintf(stderr, "Cannot open file %s\n", pathname);
		exit(1);
    	}	
	
	/*
	we need to check if the root directory could be the same as the directory
	*/

	if (!strlen(dirName)) {
		dir_inodenum = root_inodenum;
    	}
	else{
		dir_inodenum = findfile_recursive(jfs,dirName,root_inodenum,DT_DIRECTORY);
	}
	
	/*
	we need to get the i nodes of the file and the directory and the size of the directory inode
	*/	
 	
	get_inode(jfs, file_inodenum, &i_node);
	get_inode(jfs, dir_inodenum, &dir_inode);
	dir_size = dir_inode.size;

	//read in the first block of the dir
	jfs_read_block(jfs, block, dir_inode.blockptrs[0]);
	printf("Read the directory!\n");

	//find the directory of the inode and remove that directory entry
	//cast block into directory entry	
	dir_entry = (struct dirent*)block;
				
	printf("Has assigned the directory\n");	
	
	while (1) {	
		char filename[MAX_FILENAME_LEN + 1];
		memcpy(filename, dir_entry->name, dir_entry->namelen);
		filename[dir_entry->namelen] = '\0'; //segmentation fault 
		printf("%s\n",filename);

		if (!strcmp(filename, file)) {
			printf("Found the file");
			
			/*
			then find the blocks the inode is linked to and remove the links
			and also return the blocks to the free list			
			*/	
			return_inode_to_freelist(jfs, file_inodenum);
			int i=0;
			while(i_node.blockptrs[i]){
				return_block_to_freelist(jfs, i_node.blockptrs[i]);
				i++;
			}

			/*
			If the file is found then you have to create a new block and write everything into it
			before the dirent into the new block and everything after it
			then write it into the file system
			*/
			
			memcpy(newblock, block, bytes_done);
			file_before_rm = (char *)(newblock + bytes_done);
			int bytes_done_2 = bytes_done+dir_entry->entry_len;
			
			file_after_rm = (char *)(block + bytes_done_2);
			memcpy(file_before_rm, file_after_rm, (BLOCKSIZE - bytes_done_2));

			jfs_write_block(jfs, newblock, dir_inode.blockptrs[0]);
			
			jfs_commit(jfs);

			/*change the directory size*/			
			struct inode *dir_inode;
			jfs_read_block(jfs, block, inode_to_block(dir_inodenum));			
			//find the dir inode
			dir_inode = (struct inode *)(block + (dir_inodenum % INODES_PER_BLOCK)*INODE_SIZE);
			//change size to the new one
		        dir_inode->size = dir_size - dir_entry->entry_len;
			//write the block back to disk
		        jfs_write_block(jfs,block,inode_to_block(dir_inodenum));
			//commit these changes
		       	jfs_commit(jfs);
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

