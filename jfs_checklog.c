#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

FILE *open_file_for_reading(char *externalname) 
{
    FILE *fd;
    fd = fopen(externalname, "r");
    if (fd == NULL) {
	fprintf(stderr, "Cannot open file %s\n", externalname);
	exit(1);
    }
    return fd;
}

void copyin(jfs_t *jfs, char *pathname, char *externalname)
{
    int root_inode, dir_inodenum, bytes, total_bytes;
    int block_count, new_inodenum, new_blocknum;    
    int i, dirent_size;
    FILE *fd;
    char inodeblock[BLOCKSIZE], buf[BLOCKSIZE];
    char filename[MAX_FILENAME_LEN];
    char dirname[MAX_FILENAME_LEN];
    struct inode *dir_inode, *new_inode;
    root_inode = find_root_directory(jfs);
    while(pathname[0]=='/') {
	pathname++;
    }
    all_but_last_part(pathname, dirname);
    if (strlen(dirname) == 0) {
	dir_inodenum = root_inode;
    } else {
	dir_inodenum = findfile_recursive(jfs, dirname, root_inode, 
					  DT_DIRECTORY);
    }
    if (dir_inodenum < 0) {
	fprintf(stderr, "Cannot create file %s - a parent directory does not exist\n", pathname);
	exit(1);
    }

    total_bytes = 0;
    block_count = 0;
    fd = open_file_for_reading(externalname);

    /* get an inode for the file */
    new_inodenum = get_free_inode(jfs);
    if (new_inodenum < 0) {
	fprintf(stderr, "No more free inodes\n");
	exit(1);
    }
    jfs_read_block(jfs, inodeblock, inode_to_block(new_inodenum));
    new_inode = (struct inode*)(inodeblock +
                           (new_inodenum % INODES_PER_BLOCK) * INODE_SIZE);

    /* clean out the inode in case it's been used before */
    new_inode->flags = 0;
    for (i = 0; i < INODE_BLOCK_PTRS; i++) {
	new_inode->blockptrs[i] = 0;
    }

    /* read the blocks of the file in and store them*/
    while (feof(fd) == 0) {
	memset(buf, 0, BLOCKSIZE);
	bytes = fread(buf, 1, BLOCKSIZE, fd);
	total_bytes += bytes;
	if (bytes > 0) {
	    new_blocknum = get_free_block(jfs);
	    if (new_blocknum < 0) {
		fprintf(stderr, "No space on disk\n");
		//XXX should clean up here
		exit(1);
	    }
	    jfs_write_block(jfs, buf, new_blocknum);
	    new_inode->blockptrs[block_count++] = new_blocknum;

	    /* is it too big? */
	    if (block_count == INODE_BLOCK_PTRS) {
		if (feof(fd) == 0) {
		    fprintf(stderr, 
			    "Max filesize exceeded - file truncated\n");
		    break;
		}
	    }
	} else {
	    break;
	}
    }

    new_inode->size = total_bytes;

    /* write the inode back to disk */
    jfs_write_block(jfs, inodeblock, inode_to_block(new_inodenum));
    
    /* time to add this file to the directory */

    /* read in the inode for the directory */
    jfs_read_block(jfs, inodeblock, inode_to_block(dir_inodenum));
    dir_inode = (struct inode*)(inodeblock + 
                      (dir_inodenum % INODES_PER_BLOCK) * INODE_SIZE);

    /* calculate the size of the new dirent */
    last_part(pathname, filename);
    dirent_size = (((strlen(pathname)/4) + 1) * 4) + 16;

    /* create the directory entry */
    create_dirent(jfs, filename, DT_FILE, dir_inode->blockptrs[0], 
		  dir_inode->size, dirent_size, new_inodenum);

    /* update the directory inode's size */
    dir_inode->size += dirent_size;
    jfs_write_block(jfs, inodeblock, inode_to_block(dir_inodenum));

    /* commit the transaction */
    jfs_commit(jfs);
}

void usage()
{
    fprintf(stderr, 
	    "Usage: jfs_copyin <volumename> <srcfilename> <dstfilename>\n");
    exit(1);
}

int main(int argc, char **argv)
{
    struct disk_image *di;
    jfs_t *jfs;

    if (argc != 4) {
	usage();
    }

    di = mount_disk_image(argv[1]);
    jfs = init_jfs(di);

    copyin(jfs, argv[3], argv[2]);

    unmount_disk_image(di);

    exit(0);
}

