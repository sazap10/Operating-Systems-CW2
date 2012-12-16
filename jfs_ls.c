#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs_disk.h"
#include "jfs_common.h"

/* do_list_recursive lists the contents of a directory, and then
   recursively calls itself to list the contents of any
   subdirectories */

void do_list_recursive(jfs_t *jfs, char *pathname, int inodenum) 
{
    struct inode i_node;
    struct dirent* dir_entry;
    int dir_size, bytes_done=0;
    char block[BLOCKSIZE];

    get_inode(jfs, inodenum, &i_node);
    dir_size = i_node.size;

    /* read in the first block */
    jfs_read_block(jfs, block, i_node.blockptrs[0]);
    
    /* walk the directory printing out the files */
    dir_entry = (struct dirent*)block;
    while (1) {
	/* the filename isn't null-terminated on disk, so we have to
	   copy it to get a null-terminated version we can work
	   with */
	char filename[MAX_FILENAME_LEN + 1];
	memcpy(filename, dir_entry->name, dir_entry->namelen);
	filename[dir_entry->namelen] = '\0';

	if (dir_entry->file_type == DT_DIRECTORY) {
	    struct inode this_inode;
	    get_inode(jfs, dir_entry->inode, &this_inode);
	    printf("%s/%s   (directory %d bytes)\n", pathname, filename, this_inode.size);
	} else if (dir_entry->file_type == DT_FILE) {
	    struct inode this_inode;
	    get_inode(jfs, dir_entry->inode, &this_inode);
	    printf("%s/%s   %d bytes\n", pathname, filename, this_inode.size);
	} else {
	    printf("%s/%s   bad file type: %d\n", pathname, filename, dir_entry->file_type);
	}

	/* if the dirent refers to a directory, and it's not "." or
	   "..", then we want to list it's contents too */
	if (dir_entry->file_type == DT_DIRECTORY
	    && (strcmp(filename, ".") != 0)
	    && (strcmp(filename, "..") != 0)) {

	    /* extend the pathname with the name of this subdirectory*/
	    char newpathname[strlen(filename) + strlen(pathname) + 2];
	    strcpy(newpathname, pathname);
	    strcat(newpathname, "/");
	    strcat(newpathname, filename);

	    /* list the contents of the subdirectory */
	    do_list_recursive(jfs, newpathname, dir_entry->inode);
	}
	    
	bytes_done += dir_entry->entry_len;
	dir_entry = (struct dirent*)(block + bytes_done);

	/* have we finished yet? */
	if (bytes_done >= dir_size) {
	    break;
	}
    }
}

/* list_recursive lists the contents of the entire filesystem */

void list_recursive(jfs_t *jfs) 
{
    int root_inode;
    root_inode = find_root_directory(jfs);
    do_list_recursive(jfs, "", root_inode);
}

void usage() {
    fprintf(stderr, "Usage:  jfs_ls <volumename>\n");
    exit(1);
}

int main(int argc, char **argv) {
    struct disk_image *di;
    jfs_t *jfs;

    if (argc != 2) {
	usage();
    }

    /* mount the disk image file so we can access it*/
    di = mount_disk_image(argv[1]);

    jfs = init_jfs(di);

    list_recursive(jfs);

    unmount_disk_image(di);

    exit(0);
}
