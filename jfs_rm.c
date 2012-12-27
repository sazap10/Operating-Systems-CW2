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

 
/* Change the size of the dir to a new size */
void change_dir_size(jfs_t *jfs, int dir_inode_num, unsigned int new_size)
{
    char block[BLOCKSIZE];
    struct inode *dir_inode;
	//read the block in which the dir is located
    jfs_read_block(jfs,block,inode_to_block(dir_inode_num));
	//find the dir inode
	dir_inode = (struct inode *)(block + (dir_inode_num % INODES_PER_BLOCK)*INODE_SIZE);
	//change size to the new one
    dir_inode->size = new_size;
	//write the block back to disk
    jfs_write_block(jfs,block,inode_to_block(dir_inode_num));
	//commit these changes
    jfs_commit(jfs);
}

void jfs_remove_file(jfs_t *jfs,char *filename){
	struct inode file_i_node, dir_i_node;
	int root_inode,file_inode, dir_inode, dir_size, bytes_done=0;
    struct dirent* dir_entry;
    char block[BLOCKSIZE],just_filename[MAX_FILENAME_LEN], dir_name[MAX_FILENAME_LEN], new_block[BLOCKSIZE];
	//find inode number of the root directory to search for the directory and file
	root_inode = find_root_directory(jfs);
	//get the path of the directory in which the file is located
	all_but_last_part(filename,dir_name);
	//get the filename of the file from path
	last_part(filename,just_filename);
	//find the inode number of the file
	file_inode = findfile_recursive(jfs, filename, root_inode,DT_FILE);
	//file doesn't exist or a directory
	if(file_inode==-1){
		printf("rm: cannot remove '%s': No such file\n",filename);
		return;
	}
	//if directory is the root directory
	if (!strlen(dir_name)) {
		dir_inode = root_inode;
    }else{
		dir_inode = findfile_recursive(jfs,dir_name,root_inode,DT_DIRECTORY);
	}
	//get the actual inode of the file
	get_inode(jfs, file_inode, &file_i_node);
	//get the actual inode of the directory
	get_inode(jfs,dir_inode,&dir_i_node);
	dir_size = dir_i_node.size;
	//read in the first block of the dir
	jfs_read_block(jfs, block, dir_i_node.blockptrs[0]);
	//cast block into directory entry
	dir_entry = (struct dirent*)block;
	while(1){
		//correct the filename by adding the end of string char
		char file_name[MAX_FILENAME_LEN + 1];
		memcpy(file_name, dir_entry->name, dir_entry->namelen);
		file_name[dir_entry->namelen] = '\0';
		//file found
		if(!strcmp(file_name,just_filename)){
			//set the inode as free	
			return_inode_to_freelist(jfs,file_inode);
			int i =0;
			//loop through the blocks of the file
			while(file_i_node.blockptrs[i]){
				//set block as free
				return_block_to_freelist(jfs,file_i_node.blockptrs[i]);
				i++;
			}
            char *beforefile,*afterfile;
			//copy the block before the file to be deleted
			memcpy(new_block,block,bytes_done);
			//update pointer position to end of data
			beforefile = (char *)(new_block + bytes_done);
			int bytes_plus_entrylen= bytes_done+dir_entry->entry_len;
			//update pointer position to data after file
			afterfile = (char *)(block + bytes_plus_entrylen);
			//copy thi data to the new block
			memcpy(beforefile,afterfile,(BLOCKSIZE - bytes_plus_entrylen));			
			//write this new block onto disk
			jfs_write_block(jfs,new_block,dir_i_node.blockptrs[0]);
			//commit the changes
			jfs_commit(jfs);
			//change the directory size to reflect actual size
            change_dir_size(jfs, dir_inode, dir_size - dir_entry->entry_len);
			break;
		}else{
			bytes_done += dir_entry->entry_len;
			dir_entry = (struct dirent*)(block + bytes_done);
			//all data read from block
			if (bytes_done >= dir_size) {
				break;
			}
		}
	}
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

