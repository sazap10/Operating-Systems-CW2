#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

int mkdir_recursive(jfs_t *jfs, char *pathname, int inodenum) 
{
    char firstpart[MAX_FILENAME_LEN];
    struct inode i_node;
    struct dirent *dir_entry;
    int dir_size, bytes_done=0;
    char block[BLOCKSIZE];

    first_part(pathname, firstpart);

    get_inode(jfs, inodenum, &i_node);
    dir_size = i_node.size;

    /* read in the first block */
    jfs_read_block(jfs, block, i_node.blockptrs[0]);

    /* walk the directory */
    dir_entry = (struct dirent*)block;
    while (1) {
	char filename[MAX_FILENAME_LEN + 1];
	memcpy(filename, dir_entry->name, dir_entry->namelen);
	filename[dir_entry->namelen] = '\0';
	if (strcmp(firstpart, filename)==0) {
	    /* we've found the right one */
	    char newpathname[strlen(pathname)];
	    char *p;
	    p = strchr(pathname, '/');
	    if (p == NULL) {
		fprintf(stderr, "ERROR: directory already exists\n");
		exit(1);
	    }
	    /* remove the first part of the path before recursing */
	    p++;
	    strcpy(newpathname, p);
	    return mkdir_recursive(jfs, newpathname, dir_entry->inode);
	}
	    
	bytes_done += dir_entry->entry_len;
	dir_entry = (struct dirent*)(block + bytes_done);

	if (bytes_done >= dir_size) {
	    break;
	}
    }

    /* we didn't find the directory */
    /* either this is the directory we need to create, or the user
       mistyped the path */

    if (strchr(pathname, '/')==NULL) {
	/* we need to create this directory */
	int new_inodenum;
	new_inodenum = mk_new_directory(jfs, pathname, inodenum, -1);

	/* create "." and ".." entries */
	mk_new_directory(jfs, ".", new_inodenum, -1);
	mk_new_directory(jfs, "..", new_inodenum, inodenum);
	return 0;
    } else {
	/* the user mistyped the path */
	fprintf(stderr, "failed to find parent dir: %s\n", pathname);
	return -1;
    }
}

void mkdir(jfs_t *jfs, char *pathname)
{
    int root_inode;
    root_inode = find_root_directory(jfs);
    while(pathname[0]=='/') {
	pathname++;
    }
    if (mkdir_recursive(jfs, pathname, root_inode) < 0) {
	fprintf(stderr, "cannot create directory %s - a parent directory does not exist\n", pathname);
	exit(1);
    }

    /* commit all this to disk */
    jfs_commit(jfs);
}

void usage()
{
  fprintf(stderr, "Usage: jfs_mkdir <volumename> <dirname>\n");
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
    mkdir(jfs, argv[2]);

    unmount_disk_image(di);
    
    exit(0);
}

