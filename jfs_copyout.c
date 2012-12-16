#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fs_disk.h"
#include "jfs_common.h"

FILE *open_file_for_writing(char *externalname) 
{
    FILE *fd;
    fd = fopen(externalname, "w");
    if (fd == NULL) {
	fprintf(stderr, "Cannot open file %s\n", externalname);
	exit(1);
    }
    return fd;
}

void copyout(jfs_t *jfs, char *pathname, char *externalname)
{
    int root_inode, file_inodenum, file_size, bytes, blocks;
    int i;
    FILE *fd;
    struct inode i_node;
    char buf[BLOCKSIZE];
    root_inode = find_root_directory(jfs);
    while(pathname[0]=='/') {
	pathname++;
    }
    file_inodenum = findfile_recursive(jfs, pathname, root_inode, DT_FILE);
    if (file_inodenum < 0) {
	fprintf(stderr, "Cannot open file %s\n", pathname);
	exit(1);
    }

    get_inode(jfs, file_inodenum, &i_node);
    file_size = i_node.size;
    bytes = file_size;
    
    fd = open_file_for_writing(externalname);

    /* how many complete blocks are there? */
    blocks = (file_size + BLOCKSIZE - 1)/ BLOCKSIZE;

    for (i = 0; i < blocks; i++) {
	int bytes_to_write;
	int blocknum = i_node.blockptrs[i];
	jfs_read_block(jfs, buf, blocknum);

	if (bytes < BLOCKSIZE) {
	    bytes_to_write = bytes;
	} else {
	    bytes_to_write = BLOCKSIZE;
	}

	fwrite(buf, bytes_to_write, 1, fd);
	bytes -= bytes_to_write;
    }
    fclose(fd);
}

void usage()
{
  fprintf(stderr, 
	  "Usage: jfs_copyout <volumename> <srcfilename> <dstfilename>\n");
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
    copyout(jfs, argv[2], argv[3]);

    unmount_disk_image(di);

    exit(0);
}

