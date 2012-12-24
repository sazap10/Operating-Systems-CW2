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

 
/* Change the size of the specified inode to a new size */
void update_inode_size(jfs_t *jfs, int inode_num, unsigned int new_size)
{
    char block[BLOCKSIZE];
    char updatedblock[BLOCKSIZE];
    char *newblock;
    struct inode *dir_inode;
    int inodes_done = 0;
    jfs_read_block(jfs,block,inode_to_block(inode_num));
	dir_inode = (struct inode *)(block + (inode_num % INODES_PER_BLOCK)*INODE_SIZE);
    dir_inode->size = new_size;

	/*
    while(1)
    {
        dir_inode = (struct inode *)(block + inodes_done*INODE_SIZE);
        if(inode_num % INODES_PER_BLOCK == inodes_done)
        {
            dir_inode->size = new_size;
        }
        newblock = (char *)(updatedblock + inodes_done*INODE_SIZE);
        inodes_done += 1;
        memcpy(newblock,dir_inode,INODE_SIZE);
        if(inodes_done == 8)
        {
            break;
        }
    }*/
    jfs_write_block(jfs,updatedblock,inode_to_block(inode_num));
    jfs_commit(jfs);
}

void jfs_remove_file(jfs_t *jfs,char *filename){
	struct inode file_i_node, dir_i_node;
	int root_inode,file_inode, dir_inode, dir_size, bytes_done=0;
    struct dirent* dir_entry;
    char block[BLOCKSIZE],just_filename[MAX_FILENAME_LEN], rest[MAX_FILENAME_LEN], new_block[BLOCKSIZE];
	
	root_inode = find_root_directory(jfs);
	
	all_but_last_part(filename,rest);
	last_part(filename,just_filename);
	file_inode = findfile_recursive(jfs, filename, root_inode,DT_FILE);
	if(file_inode==-1){
		printf("rm: cannot remove '%s': No such file\n",filename);
		exit(1);
	}
	
	dir_inode = findfile_recursive(jfs,rest,root_inode,DT_DIRECTORY);
	
	get_inode(jfs, file_inode, &file_i_node);
	
	get_inode(jfs,dir_inode,&dir_i_node);
	dir_size = dir_i_node.size;
	
	jfs_read_block(jfs, block, dir_i_node.blockptrs[0]);
	
	dir_entry = (struct dirent*)block;
	while(1){
		char file_name[MAX_FILENAME_LEN + 1];
		memcpy(file_name, dir_entry->name, dir_entry->namelen);
		file_name[dir_entry->namelen] = '\0';
		if(!strcmp(file_name,just_filename)){
            char *beforefile;
            char *afterfile;
			memcpy(new_block,block,bytes_done);
			beforefile = (char *)(new_block + bytes_done);
			int bytes_plus_entrylen= bytes_done+dir_entry->entry_len;
			afterfile = (char *)(block + bytes_plus_entrylen);
			memcpy(beforefile,afterfile,(BLOCKSIZE - bytes_plus_entrylen));			
			jfs_write_block(jfs,new_block,dir_i_node.blockptrs[0]);
			jfs_commit(jfs);
            update_inode_size(jfs, dir_inode, dir_size - dir_entry->entry_len);
			//set the inode as free	
			return_inode_to_freelist(jfs,file_inode);
			int i =0;
	
			while(file_i_node.blockptrs[i]){
				//set block as free
				return_block_to_freelist(jfs,file_i_node.blockptrs[i]);
				i++;
			}
			
			break;
			//remove it
		}else{
			bytes_done += dir_entry->entry_len;
			dir_entry = (struct dirent*)(block + bytes_done);

			if (bytes_done >= dir_size) {
				break;
			}
		}
	}
	
	
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

