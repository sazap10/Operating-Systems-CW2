#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
 
#include "fs_disk.h"
#include "jfs_common.h"
 
/* Change the size of the specified inode to a new size */
void update_inode_size(jfs_t *jfs, int inode_num, unsigned int new_size)
{
    char block[BLOCKSIZE];
    char updatedblock[BLOCKSIZE];
    char *newblock;
    struct inode *dir_inode;
    int inodes_done = 0;
    jfs_read_block(jfs,block,inode_to_block(inode_num));
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
    }
    jfs_write_block(jfs,updatedblock,inode_to_block(inode_num));
    jfs_commit(jfs);
}
 
 
/* Rewrite the block with the dirent removed - ASSUMING FILE IS IN ROOT */
void remove_dirent_recursive(jfs_t *jfs, char *pathname, int inode_num)
{
    int dir_size;
    struct inode dir_inode;
    struct dirent *dir_entry;
    char block[BLOCKSIZE];
    char target_dirent[(sizeof(struct dirent)+MAX_FILENAME_LEN)];
     
    /* Get inode of the directory and read directory into 'block' variable */
    get_inode(jfs, inode_num, &dir_inode);
    jfs_read_block(jfs, block, dir_inode.blockptrs[0]);
    int bytes_done = 0;
 
    dir_size = dir_inode.size;
 
    /* dir_entry is a pointer to the dirent in the block */
    dir_entry = (struct dirent*)block;
 
    /* target_dirent is the dirent name we want to find in this recursion */
    first_part(pathname,target_dirent);
 
    while(1)
    {
        char filename[MAX_FILENAME_LEN + 1];
        memcpy(filename, dir_entry->name, dir_entry->namelen);
        filename[dir_entry->namelen] = '\0';
        if(strcmp(filename,target_dirent)==0)
        {
            if(strcmp(pathname,target_dirent)==0)
            {
                /* We've found the dirent up for deletion */
                /* Write a new block with the dirent not there. */
                char updatedblock[BLOCKSIZE];
                char *newblock;
                char *restofblock;
                memcpy(updatedblock,block,bytes_done);
                newblock = (char *)(updatedblock + bytes_done);
                restofblock = (char *)(block + (bytes_done + dir_entry->entry_len));
                memcpy(newblock,restofblock,(BLOCKSIZE - bytes_done - dir_entry->entry_len));
                jfs_write_block(jfs, updatedblock, dir_inode.blockptrs[0]);
                jfs_commit(jfs);
                /* Now all that's left is to edit the inode of the directory to update its size */
                unsigned int new_size = dir_size - dir_entry->entry_len;
                update_inode_size(jfs, inode_num, new_size);
            }
            else
            {
                /* There is more path to walk */
                char *p;
                p = strchr(pathname, '/');
                p++;
                remove_dirent_recursive(jfs,p,dir_entry->inode);
                return;
            }
        }
        bytes_done += dir_entry->entry_len;
        dir_entry = (struct dirent*)(block + bytes_done);
 
        /* have we finished yet? */
        if (bytes_done >= dir_size) {
             break;
        }
    }
}
 
void removefile(jfs_t *jfs, char *pathname)
{
    int root_inodenum, file_inodenum, counter;
    struct inode file_inode;
 
    root_inodenum = find_root_directory(jfs);
 
    while(pathname[0]=='/') {
        pathname++;
    }
    /* Get the inode number of the file */
    file_inodenum = findfile_recursive(jfs, pathname, root_inodenum, DT_FILE);
    /* Check if file exists */
    if (file_inodenum < 0) {
        fprintf(stderr, "Cannot find file %s\n", pathname);
        exit(1);
    }
    /* Get the actual inode of the file */
    get_inode(jfs, file_inodenum, &file_inode);
 
    /* For all the block pointers in the inode set them to unused */
    for(counter = 0; counter < 14; counter++)
    {
        if(file_inode.blockptrs[counter] != NULL)
        {
            return_block_to_freelist(jfs,file_inode.blockptrs[counter]);
        }
    }
    /* Set the inode to unused */
    return_inode_to_freelist(jfs, file_inodenum);
 
    /* Get rid of the dirent of the file */
    remove_dirent_recursive(jfs, pathname, root_inodenum);
}
 
 
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
 
     if (argc != 3) {
        usage();
     }
 
    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);
     removefile(jfs, argv[2]);
    unmount_disk_image(di);
 
    exit(0);
}