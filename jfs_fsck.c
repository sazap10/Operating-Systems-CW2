#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "fs_disk.h"
#include "jfs_common.h"

void walk_filesystem(jfs_t *jfs, char *pathname, int inodenum,
		     char *block_bitmap, char *inode_bitmap) 
{
    struct inode i_node;
    struct dirent* dir_entry;
    int dir_size, bytes_done=0;
    char block[BLOCKSIZE];

    /*    inode_bitmap[inodenum] = 2;*/
    get_inode(jfs, inodenum, &i_node);
    dir_size = i_node.size;

    /* read in the first block */
    jfs_read_block(jfs, block, i_node.blockptrs[0]);
    block_bitmap[i_node.blockptrs[0]]++;
    
    /* walk the directory printing out the files */
    dir_entry = (struct dirent*)block;
    while (1) {
	char filename[MAX_FILENAME_LEN + 1];
	memcpy(filename, dir_entry->name, dir_entry->namelen);
	filename[dir_entry->namelen] = '\0';
	if (dir_entry->file_type == DT_DIRECTORY) {
	    struct inode this_inode;
	    get_inode(jfs, dir_entry->inode, &this_inode);
	} else if (dir_entry->file_type == DT_FILE) {
	    struct inode this_inode;
	    int i;
	    inode_bitmap[dir_entry->inode]++;
	    get_inode(jfs, dir_entry->inode, &this_inode);
	    for (i = 0; i < INODE_BLOCK_PTRS; i++) {
		if (this_inode.blockptrs[i] >= 0) {
		    block_bitmap[this_inode.blockptrs[i]]++;
		}
	    }
	} else {
	    printf("%s/%s   bad file type: %d\n", pathname, filename, dir_entry->file_type);
	}
	if (dir_entry->file_type == DT_DIRECTORY
	    && (strcmp(filename, ".") != 0)
	    && (strcmp(filename, "..") != 0)) {

	    char newpathname[strlen(filename) + strlen(pathname) + 2];

	    /* don't count the inode links for . or .. because they
	       should have already been counted */
	    inode_bitmap[dir_entry->inode]++;

	    strcpy(newpathname, pathname);
	    strcat(newpathname, "/");
	    strcat(newpathname, filename);
	    walk_filesystem(jfs, newpathname, dir_entry->inode,
			    block_bitmap, inode_bitmap);
	}
	    
	bytes_done += dir_entry->entry_len;
	dir_entry = (struct dirent*)(block + bytes_done);

	if (bytes_done >= dir_size) {
	    break;
	}
    }
}

void build_bitmaps(jfs_t *jfs, char *block_bitmap, 
		   int no_of_blocks, char *inode_bitmap, int no_of_inodes) 
{
    int root_inode, i, j, groups;
    memset(block_bitmap, 0, no_of_blocks);
    memset(inode_bitmap, 0, no_of_blocks);
    root_inode = find_root_directory(jfs);

    /* count the inode for root here because otherwise it will be missed */
    inode_bitmap[root_inode]++;

    /* walk the entire filesystem counting used blocks and inodes */
    walk_filesystem(jfs, "", root_inode, block_bitmap, inode_bitmap);

    /* how many blockgroups are there ? */
    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }
    
    /* count the blocks used by superblocks and freelist bitmaps */
    for (i = 0; i < groups; i++) {
	/* the superblock is used, plus the inode blocks after it */
	for(j = 0; j < DATA_BLOCK_OFFSET; j++) {
	    block_bitmap[i*BLOCKS_PER_GROUP + j] = 1;
	}
    }    
}

void build_free_bitmaps(jfs_t *jfs, char *block_bitmap, 
			int no_of_blocks, 
			char *inode_bitmap, int no_of_inodes) 
{
    int groups, i, j;
    memset(block_bitmap, 0, no_of_blocks);
    memset(inode_bitmap, 0, no_of_blocks);
    

    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }
    
    for (i = 0; i < groups; i++) {
	char block[BLOCKSIZE];
	struct blockgroup *grp;
	jfs_read_block(jfs, block, i*BLOCKS_PER_GROUP);
	grp = (struct blockgroup*)block;
	for(j = 0; j < BLOCKS_PER_GROUP; j++) {
	    if ((grp->block_bitmap[j/8] & (0x80 >> (j%8))) != 0) {
		block_bitmap[i*BLOCKS_PER_GROUP + j] = 1;
	    }
	}
	for(j = 0; j < INODES_PER_GROUP; j++) {
	    if ((grp->inode_bitmap[j/8] & (0x80 >> (j%8))) != 0) {
		inode_bitmap[i*INODES_PER_GROUP + j] = 1;
	    }
	}
    }
}

void usage() {
    fprintf(stderr, "Usage:  jfs_fsck <volumename>\n");
    exit(1);
}

int main(int argc, char **argv) {
  struct disk_image *di;
  jfs_t *jfs;
  char *block_bitmap, *inode_bitmap;
  char *free_block_bitmap, *free_inode_bitmap;
  int no_of_blocks, no_of_inodes, i, errors;

  if (argc != 2) {
      usage();
  }


  di = mount_disk_image(argv[1]);

  jfs = init_jfs(di);

  /* create "bitmaps" for used blocks and blocks the freelist says are used */

  /* use one byte per block, so we can detect multiply counted blocks */
  no_of_blocks = jfs->d_img->size / BLOCKSIZE;
  block_bitmap = malloc(no_of_blocks);
  free_block_bitmap = malloc(no_of_blocks);

  /* create "bitmaps" for used inodes and inodes the freelist says are used */

  /* use one byte per inode */
  no_of_inodes = (jfs->d_img->size * INODES_PER_GROUP)
      /(BLOCKSIZE * BLOCKS_PER_GROUP);
  inode_bitmap = malloc(no_of_inodes);
  free_inode_bitmap = malloc(no_of_inodes);

  /* find out what blocks and inodes the directory tree thinks are in use */
  build_bitmaps(jfs, block_bitmap, no_of_blocks, inode_bitmap, no_of_inodes);

  /* find out what blocks and inodes the freelist bitmaps think are in use */
  build_free_bitmaps(jfs, free_block_bitmap, no_of_blocks, 
		     free_inode_bitmap, no_of_inodes);


  /* compare the two views */
  errors = 0;
  for (i=0; i < no_of_inodes; i++) {
      if (inode_bitmap[i] > 1) {
	  printf("Inode %d is used %d times\n", i, inode_bitmap[i]);
	  errors++;
      }
      if (inode_bitmap[i] > 0 && free_inode_bitmap[i] == 0) {
	  printf("Inode %d is used, but is listed as being free\n", i);
	  errors++;
      }
      if (inode_bitmap[i] == 0 && free_inode_bitmap[i] > 0) {
	  printf("Inode %d is not used, but is not listed as being free\n", i);
	  errors++;
      }
  }
  for (i=0; i < no_of_blocks; i++) {
      if (block_bitmap[i] > 1) {
	  printf("Block %d is used %d times\n", i, block_bitmap[i]);
	  errors++;
      }
      if (block_bitmap[i] > 0 && free_block_bitmap[i] == 0) {
	  printf("Block %d is used, but is listed as being free\n", i);
	  errors++;
      }
      if (block_bitmap[i] == 0 && free_block_bitmap[i] > 0) {
	  printf("Block %d is not used, but is not listed as being free\n", i);
	  errors++;
      }
  }
  if (errors == 0) {
      printf("Filesystem '%s' is consistent\n", argv[1]);
  }

/*#define DEBUG*/
#ifdef DEBUG
  /* you can define DEBUG if you want a verbose view of the disk for
     debugging purposes */

  printf("The directory view of the disk:\n");
  printf("Used i-nodes: ");
  for (i=0; i < no_of_inodes; i++) {
      if (inode_bitmap[i]>0) {
	  printf(", %d", i);
      }
      if (inode_bitmap[i]>1) {
	  printf("(%d)", inode_bitmap[i]);
      }
  }
  printf("\n");
  printf("Used blocks: ");
  for (i=0; i < no_of_blocks; i++) {
      if (block_bitmap[i]>0) {
	  printf(", %d", i);
      }
      if (block_bitmap[i]>1) {
	  printf("(%d)", block_bitmap[i]);
      }
  }
  printf("\n\n");

  printf("The freelist view of the disk:\n");
  printf("Used i-nodes: ");
  for (i=0; i < no_of_inodes; i++) {
      if (free_inode_bitmap[i]>0) {
	  printf(", %d", i);
      }
  }
  printf("\n");
  printf("Used blocks: ");
  for (i=0; i < no_of_blocks; i++) {
      if (free_block_bitmap[i]>0) {
	  printf(", %d", i);
      }
  }
  printf("\n");
#endif

  unmount_disk_image(di);

  exit(0);
}
