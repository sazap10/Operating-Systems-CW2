#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs_disk.h"
#include "jfs_common.h"

/* raw_create_dirent creates an entry in a directory, and saves this
   to disk.  It needs to be supplied with the filename, filetype and
   file's inode, and the block number of the directory, the offset in
   bytes within the directory, and the length of the directory entry
   to create.  Normally create_dirent in jfs_common.c should be
   used, but this version is here to bypass the logfile while we're
   actually creating the logfile. */

void raw_create_dirent(struct disk_image *di, char *filename, 
		       int type, int blocknum, int offset,
		       int length, int inodenum) 
{
    char block[BLOCKSIZE];
    struct dirent *d_ent;
    read_block(di, block, blocknum);

    if (offset + sizeof(struct dirent) + strlen(filename)+1 > BLOCKSIZE) {
	fprintf(stderr, "No more space in directory\n");
	return;
    }
    d_ent = (struct dirent*)(block+offset);
    
    if (strlen(filename) > MAX_FILENAME_LEN) {
	fprintf(stderr, "Filename %s too long\n", filename);
	return;
    }
    memcpy(d_ent->name, filename, strlen(filename));
    d_ent->namelen = strlen(filename);
    d_ent->file_type = type;
    d_ent->inode = inodenum;
    d_ent->entry_len = length;

    write_block(di, block, blocknum);
}

/* raw_get_free_inode is the same as get_free_inode, but bypasses the
   JFS logfile.  This should only be used for creating the logfile in
   the first place. */

int raw_get_free_inode(struct disk_image *di) 
{
    int groups, i;

    if (di->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    for (i = 0; i < groups; i++) {
	struct blockgroup *grp;
	char block[BLOCKSIZE];
	int freeinode;
	/* read the blockgroup header */
	read_block(di, block, i * BLOCKS_PER_GROUP);
	grp = (struct blockgroup *)block;
	freeinode = -1;
	freeinode = find_free_inode(grp->inode_bitmap);
	if (freeinode >= 0) {
	    /* if there's a free block in this group, then return it */
	    set_inode_used(grp->inode_bitmap, freeinode);
	    write_block(di, block, i * BLOCKS_PER_GROUP);
	    freeinode += i * INODES_PER_GROUP;
	    /* printf("allocating free inode: %d\n", freeinode);*/
	    return freeinode;
	} 
    }
    return -1;
}

int raw_get_free_block(struct disk_image *di) 
{
    int groups, i;

    if (di->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    for (i = 0; i < groups; i++) {
	struct blockgroup *grp;
	char block[BLOCKSIZE];
	int freeblock;
	/* read the blockgroup header */
	read_block(di, block, i * BLOCKS_PER_GROUP);
	grp = (struct blockgroup *)block;
	freeblock = -1;
	freeblock = find_free_block(grp->block_bitmap);
	if (freeblock >= 0) {
	    set_block_used(grp->block_bitmap, freeblock);
	    write_block(di, block, i * BLOCKS_PER_GROUP);
	    /* if there's a free block in this group, then return it */
	    freeblock += i * BLOCKS_PER_GROUP;
	    return freeblock;
	} 
    }
    return -1;
}

/* raw_mk_new_directory is basically the same as mk_new_directory, but
   it bypasses the jfs logfile.  It's used to create the root
   directory before we've got a logfile.  Normal filesystem operations
   should not use this function */

int raw_mk_new_directory(struct disk_image *di, char *pathname, 
		      int parent_inodenum, int grandparent_inodenum) 
{
    /* we need to create this directory */
    /* round size up to nearest 4 byte boundary */
    int size, prev_size, bytes_done=0;
    int new_inodenum, new_blocknum;
    char block[BLOCKSIZE];
    struct inode* new_inode, *parent_inode;

    /* calculate the size of the new parent dirent */
    size = (((strlen(pathname)/4) + 1) * 4) + 16;

    /* does it fit?*/
    if (bytes_done + size > BLOCKSIZE) {
	fprintf(stderr,
		"No more space in the directory to add another entry\n");
	exit(1);
    }

    /* allocate an inode for this directory */
    if (strcmp(pathname, ".")==0) {
	new_inodenum = parent_inodenum;
    } else if (strcmp(pathname, "..")==0) {
	new_inodenum = grandparent_inodenum;
    } else {
	new_inodenum = raw_get_free_inode(di);
    }


    /* update the parent directories inode size field */
    read_block(di, block, inode_to_block(parent_inodenum));
    parent_inode = (struct inode*)(block + (parent_inodenum % INODES_PER_BLOCK)
				   * INODE_SIZE);

    prev_size = parent_inode->size;
    parent_inode->size += size;
    write_block(di, block, inode_to_block(parent_inodenum));

    /* create an entry in the parent directory */
    raw_create_dirent(di, pathname, DT_DIRECTORY, parent_inode->blockptrs[0],
		  prev_size, size, new_inodenum);


    if (strcmp(pathname, ".")!=0 && strcmp(pathname, "..")!=0) {
	/* it's a real directory */

	/* get a block to hold the directory */
	new_blocknum = raw_get_free_block(di);

	/* read in the block that contains our new inode */
	read_block(di, block, inode_to_block(new_inodenum));
	
	new_inode = (struct inode*)(block + (new_inodenum % INODES_PER_BLOCK)
				    * INODE_SIZE);
	new_inode->flags = FLAG_DIR;
	new_inode->blockptrs[0] = new_blocknum;
	new_inode->size = 0;
	
	/* write back the inode block */
	write_block(di, block, inode_to_block(new_inodenum));
	return new_inodenum;
    }

    return parent_inodenum;
}

/* create_new_group creates a new group of blocks on the virtual disk,
   including a new superblock and bitmaps of used blocks and used
   inodes */

void create_new_group(struct disk_image *di, int blocknum) 
{
    char block[BLOCKSIZE];
    struct blockgroup *grp;
    int i;

    grp = (struct blockgroup*)block;
    grp->superblock.size = di->size / BLOCKSIZE;
    grp->superblock.free_blocks = 0; /* we'll update this later */
    grp->superblock.free_inodes = 0; /* we'll update this later */
    grp->superblock.first_inode = 0; /* we'll update this later */

    /* clear the blocks bitmap */
    memset(grp->block_bitmap, 0, BLOCKS_PER_GROUP/8);

    /* record in the block_bitmap that we've used the superblock and
       the blocks used for inodes */
    for (i=0; i< DATA_BLOCK_OFFSET; i++) {
	set_block_used(grp->block_bitmap, i);
    }

    /* clear the inodes bitmap */
    memset(grp->inode_bitmap, 0, INODES_PER_GROUP/8);

    /* write the new superblock back to disk */
    write_block(di, block, blocknum);
}

/* create_fs_root creates a new root directory on the virtual disk */

void create_fs_root(struct disk_image *di)
{
  int inode_num, blocknum, i, inode_block;
  struct inode *i_node;
  struct blockgroup *grp;

  /* there's no directory there yet, so we can just write a new one -
     we don't need to read the block before we write it */
  char block[BLOCKSIZE];
  memset(block, 0, BLOCKSIZE);

  /* find a free i-node for the directory*/
  inode_num = raw_get_free_inode(di);

  /* find a free block to hold the directory's files */
  blocknum = raw_get_free_block(di);


  /* now we need to create the i-node */

  i_node = (struct inode*)(block + (inode_num%INODES_PER_BLOCK) * INODE_SIZE);
  i_node->size = 0;
  i_node->flags = FLAG_DIR;

  /* clear the block pointers */
  for (i = 0; i<INODE_BLOCK_PTRS; i++) {
    i_node->blockptrs[i] = -1;
  }
  
  /* the first block holds the directory */
  i_node->blockptrs[0] = blocknum;

  /* where does the i-node go on disk? */
  inode_block = inode_to_block(inode_num);

  write_block(di, block, inode_block);

  /* now create the "." and ".." directories */
  /* these are real directory entries, but the inode is the same as
     the root directory */
  raw_mk_new_directory(di, ".", inode_num, -1);
  raw_mk_new_directory(di, "..", inode_num, inode_num);


  /* update the superblock */
  read_block(di, block, 0);
  grp = (struct blockgroup*)block;
  grp->superblock.free_blocks--; 
  grp->superblock.free_inodes--; 
  /* store the root directory in the superblock */
  grp->superblock.first_inode = inode_num;
  /* write the superblock and bitmaps back to disk */
  write_block(di, block, 0);
}

/* clone_superblocks copies the master superblock into all the other
   superblocks in the other block groups to preserve the data in case
   of corruption - in reality corruption isn't likely in a filesystem
   in a file */

int clone_superblocks(struct disk_image *di)
{
    /* should implement this someday! */
    return 0;
}

/* create_new_filesystem creates the entire filesystem on the virtual disk */
int create_new_filesystem(struct disk_image *di) 
{
    int groups, i;

    if (di->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + di->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }
    
    for (i = 0; i < groups; i++) {
	create_new_group(di, i * BLOCKS_PER_GROUP);
    }

    create_fs_root(di);
    clone_superblocks(di);
    return 0;
}



int create_log_file(struct disk_image* di)
{
    int rootdir_inodenum;
    int log_inodenum, new_blocknum;    
    int i, dirent_size;
    char inodeblock[BLOCKSIZE], buf[BLOCKSIZE];
    struct inode *rootdir_inode, *log_inode;
    char *logfilename = ".log";
    struct blockgroup *grp;

    /* find the root directory by reading the superblock */
    read_block(di, buf, 0);
    grp = (struct blockgroup*)buf;
    rootdir_inodenum = grp->superblock.first_inode;

    /* get an inode for the file */
    log_inodenum = raw_get_free_inode(di);
    /* use raw unbuffered interface for this */
    read_block(di, inodeblock, inode_to_block(log_inodenum));
    log_inode = (struct inode*)(inodeblock +
				(log_inodenum % INODES_PER_BLOCK) * INODE_SIZE);
    /* clean out the inode to be safe */
    log_inode->flags = 0;
    for (i = 0; i < INODE_BLOCK_PTRS; i++) {
	log_inode->blockptrs[i] = -1;
    }

    /* allocate the blocks of the log file */
    for(i = 0; i < LOGFILE_BLOCKS; i++) {
	/* zero the blocks, just to be safe */
	memset(buf, 0, BLOCKSIZE);
	new_blocknum = raw_get_free_block(di);
	if (new_blocknum < 0) {
	    /* this should never happen */
	    fprintf(stderr, "No space on disk for logfile\n");
	    exit(1);
	}
	/* write the block using the raw unbuffered interface */
	write_block(di, buf, new_blocknum);
	log_inode->blockptrs[i] = new_blocknum;
    }

    log_inode->size = LOGFILE_BLOCKS * BLOCKSIZE;

    /* write the inode back to disk, using raw interface bypassing journal */
    write_block(di, inodeblock, inode_to_block(log_inodenum));
    
    /* time to add this file to the directory */

    /* read in the inode for the directory */
    read_block(di, inodeblock, inode_to_block(rootdir_inodenum));
    rootdir_inode = (struct inode*)(inodeblock + 
                      (rootdir_inodenum % INODES_PER_BLOCK) * INODE_SIZE);

    /* calculate the size of the new dirent */
    dirent_size = (((strlen(logfilename)/4) + 1) * 4) + 16;

    /* create the directory entry */
    raw_create_dirent(di, logfilename, DT_FILE, rootdir_inode->blockptrs[0], 
		      rootdir_inode->size, dirent_size, log_inodenum);

    /* update the directory inode's size */
    rootdir_inode->size += dirent_size;
    write_block(di, inodeblock, inode_to_block(rootdir_inodenum));
    return 0;
}

void usage() {
    fprintf(stderr, "Usage:  jfs_format <volumename>\n");
    exit(1);
}

int main(int argc, char **argv)
{
  struct disk_image *di;

  if (argc != 2) {
      usage();
  }

  /* create a new disk image file */
  create_disk_image(argv[1], 1024 * BLOCKSIZE);

  /* mount the disk image file so we can access it*/
  di = mount_disk_image(argv[1]);

  /* create the basic filesystem */
  create_new_filesystem(di);

  /* create the JFS logfile */
  create_log_file(di);


  /* unmount the disk image, and force any remaining writes to
     happen */
  unmount_disk_image(di);

  exit(0);
}
