#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "fs_disk.h"
#include "jfs_common.h"

/* for testing purposes only */
/* set this to x and the program will terminate after x blocks have
   been written */
int crash_after = -1;

/* first_part takes a pathname, and extracts the first part of the
   pathname before the first slash */

void first_part(char *pathname, char *firstpart) {
    char *p = pathname;
    char *p2;

    /* move past any leading slashes */
    if (*p == '/') {
	p++;
	if (*p == '\0') {
	    /* this must be the root directory */
	    firstpart[0]='\0';
	}
    }
    
    strcpy(firstpart, p);
    p2 = strchr(firstpart, '/');
    if (p2 == NULL) {
	/* the first part was the last part */
    } else {
	/* overwrite the / with a string terminator */
	*p2 = '\0';
    }
    return;
}

/* last_part takes a pathname, and extracts the last part of the
   pathname after the last slash */

void last_part(char *pathname, char *lastpart) {
    char *p = pathname;
    char *p2 = strchr(p, '/');
    if (p2 == NULL) {
	strcpy(lastpart, pathname);
	return;
    }
    while (p2 != NULL) {
	p = p2+1;
	p2 = strchr(p, '/');
    }
    strcpy(lastpart, p);
    return;
}

/* last_part takes a pathname, and extracts the last part of the
   pathname after the last slash */

void all_but_last_part(char *pathname, char *rest) {
    int len;
    char *p = pathname;
    char *p2 = strchr(p, '/');
    if (p2 == NULL) {
	/* return empty string */
	rest[0] = '\0';
	return;
    }
    while (p2 != NULL) {
	p = p2+1;
	p2 = strchr(p, '/');
    }
    len = p - pathname - 1;
    memcpy(rest, pathname, len);
    rest[len] = '\0';
    return;
}

/* get_dirent_len gets the actual length of the dirent, as stored in a
   "disk" block.  This make be different from sizeof(struct dirent)
   and from the value stored in entry_len */

int get_dirent_len(struct dirent* d_ent)
{
    int len = (int)(&(d_ent->name[0])) - (int)d_ent;
    /* add the length of the name, rounded up to the nearest 32-bit boundary */
    len += ((d_ent->namelen / 4) + 1) * 4;
    return len;
}

/* create_dirent creates an entry in a directory, and saves this to
   disk.  It needs to be supplied with the filename, filetype and
   file's inode, and the block number of the directory, the offset in
   bytes within the directory, and the length of the directory entry
   to create */

void create_dirent(jfs_t *jfs, char *filename, 
		   int type, int blocknum, int offset,
		   int length, int inodenum) 
{
    char block[BLOCKSIZE];
    struct dirent *d_ent;
    jfs_read_block(jfs, block, blocknum);

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

    jfs_write_block(jfs, block, blocknum);
}

/* find_free_block is used by get_free_block to locate a free block
   within a single blockgroup only.  You probably don't want to
   directly use this function, but use get_free_block instead */

int find_free_block(unsigned char *block_bitmap) 
{
    int i, j;
    for (i =0; i< BLOCKS_PER_GROUP/8; i++) {
        unsigned char bm;
	if (block_bitmap[i] == 0xff) {
	    /* no space left in this block */
	    continue;
	}

	/* this byte indicates some space somewhere */
	bm = block_bitmap[i];
	for (j=0; i<8; j++) {
	    if ((bm & 0x80) == 0) {
		/* found the free block! */
		return (i*8) + j;
	    } else {
		bm <<= 1;
	    }
	}
    }
    
    /* no space left in this group */
    return -1;
}

/* set_block_used simply sets the relevant bit in a free block bitmap.
   Mostly it's used by get_free_block */

void set_block_used(unsigned char *block_bitmap, int blocknum)
{
    unsigned char bm;
    bm = block_bitmap[blocknum/8];
    bm |= (0x80 >> (blocknum % 8));
    block_bitmap[blocknum/8] = bm;
}

/* set_block_used simply unsets the relevant bit in a free block bitmap.
   Mostly it's used by return_block_to_freelist */

void set_block_unused(unsigned char *block_bitmap, int blocknum)
{
    unsigned char bm;
    bm = block_bitmap[blocknum/8];
    /* set it to 1 */
    bm |= (0x80 >> (blocknum % 8));
    /* use XOR to clear it */
    bm ^= (0x80 >> (blocknum % 8));
    block_bitmap[blocknum/8] = bm;
}

/* find_free_inode is used get get_free_inode to locate a free inode
   within a single blockgroup only.  You probably don't want to
   directly use this function, but use get_free_inode instead */

int find_free_inode(unsigned char *inode_bitmap) 
{
    int i, j;
    for (i =0; i< INODES_PER_GROUP/8; i++) {
	unsigned char bm;
	if (inode_bitmap[i] == 0xff) {
	    /* no space left in this block */
	    continue;
	}

	/* this byte indicates some space somewhere */
	bm = inode_bitmap[i];
	for (j=0; i<8; j++) {
	    if ((bm & 0x80) == 0) {
		/* found the free block! */
		return (i*8) + j;
	    } else {
		bm <<= 1;
	    }
	}
    }
    
    /* no space left in this group */
    return -1;
}

/* set_inode_used simply sets the relevant bit in a free inode bitmap.
   Mostly it's used by get_free_inode */

void set_inode_used(unsigned char *inode_bitmap, int inodenum)
{
    unsigned char bm;
    bm = inode_bitmap[inodenum/8];
    bm |= (0x80 >> (inodenum % 8));
    inode_bitmap[inodenum/8] = bm;
}

/* set_inode_unused simply unsets the relevant bit in a free block bitmap.
   Mostly it's used by return_inode_to_freelist */

void set_inode_unused(unsigned char *inode_bitmap, int inodenum)
{
    unsigned char bm;
    bm = inode_bitmap[inodenum/8];
    /* set the bit */
    bm |= (0x80 >> (inodenum % 8));
    /* now clear it using XOR */
    bm ^= (0x80 >> (inodenum % 8));
    inode_bitmap[inodenum/8] = bm;
}

/* count_free_blocks counts the number of free blocks in a
   blockgroup's free block bitmap */

int count_free_blocks(unsigned char *block_bitmap) 
{
    int i, j;
    int count;
    for (i =0; i< BLOCKS_PER_GROUP/8; i++) {
	unsigned char bm;
	if (block_bitmap[i] == 0xff) {
	    /* no space left in this block */
	    continue;
	}

	/* this byte indicates some space somewhere */
	bm = block_bitmap[i];
	for (j=0; i<8; j++) {
	    if ((bm & 0x80) == 0) {
		/* found a free block! */
		count++;
	    } else {
		bm <<= 1;
	    }
	}
    }
    
    return count;
}

/* get_free_block is the main way to get a block allocated for your
   usage.  It finds an unused block, reserves it in the free block
   bitmap, and returns the number of the block.  If there are no free
   blocks available because the "disk" is full, it returns -1 */

int get_free_block(jfs_t *jfs) 
{
    int groups, i;

    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    for (i = 0; i < groups; i++) {
	struct blockgroup *grp;
	char block[BLOCKSIZE];
	int freeblock;
	/* read the blockgroup header */
	jfs_read_block(jfs, block, i * BLOCKS_PER_GROUP);
	grp = (struct blockgroup *)block;
	freeblock = -1;
	freeblock = find_free_block(grp->block_bitmap);
	if (freeblock >= 0) {
	    set_block_used(grp->block_bitmap, freeblock);
	    jfs_write_block(jfs, block, i * BLOCKS_PER_GROUP);
	    /* if there's a free block in this group, then return it */
	    freeblock += i * BLOCKS_PER_GROUP;
	    /* printf("allocating free block: %d\n", freeblock);*/
	    return freeblock;
	} 
    }
    return -1;
}

/* get_free_inode is the main way to get a inode allocated for your
   usage.  It finds an unused inode, reserves it in the free inode
   bitmap, and returns the number of the inode.  If there are no free
   inodes available because the "disk" is full, it returns -1 */

int get_free_inode(jfs_t *jfs) 
{
    int groups, i;

    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    for (i = 0; i < groups; i++) {
	struct blockgroup *grp;
	char block[BLOCKSIZE];
	int freeinode;
	/* read the blockgroup header */
	jfs_read_block(jfs, block, i * BLOCKS_PER_GROUP);
	grp = (struct blockgroup *)block;
	freeinode = -1;
	freeinode = find_free_inode(grp->inode_bitmap);
	if (freeinode >= 0) {
	    /* if there's a free block in this group, then return it */
	    set_inode_used(grp->inode_bitmap, freeinode);
	    jfs_write_block(jfs, block, i * BLOCKS_PER_GROUP);
	    freeinode += i * INODES_PER_GROUP;
	    /* printf("allocating free inode: %d\n", freeinode);*/
	    return freeinode;
	} 
    }
    return -1;
}

void return_inode_to_freelist(jfs_t *jfs, int inodenum) 
{
    int groups, blockgroup;
    struct blockgroup *grp;
    char block[BLOCKSIZE];
    int freeinode;

    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    /* which blockgroup is it in */
    blockgroup = inodenum/INODES_PER_GROUP;

    /* read the blockgroup header */
    jfs_read_block(jfs, block, blockgroup * BLOCKS_PER_GROUP);
    grp = (struct blockgroup *)block;

    /* which inode is it within this block group */
    freeinode = inodenum % INODES_PER_GROUP;

    /* clear it and write the bitmap back */
    set_inode_unused(grp->inode_bitmap, freeinode);
    jfs_write_block(jfs, block, blockgroup * BLOCKS_PER_GROUP);
}

void return_block_to_freelist(jfs_t *jfs, int blocknum) 
{
    int groups, blockgroup, freeblock;
    struct blockgroup *grp;
    char block[BLOCKSIZE];

    if (jfs->d_img->size % (BLOCKSIZE * BLOCKS_PER_GROUP) == 0) {
	groups = jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    } else {
	groups = 1 + jfs->d_img->size / (BLOCKSIZE * BLOCKS_PER_GROUP);
    }

    /* which blockgroup is it in */
    blockgroup = blocknum/BLOCKS_PER_GROUP;

    /* read the blockgroup header (first block in group) */
    jfs_read_block(jfs, block, blockgroup * BLOCKS_PER_GROUP);
    grp = (struct blockgroup *)block;

    /* which block is it within this block group */
    freeblock = blocknum % BLOCKS_PER_GROUP;

    /* clear it and write the bitmap back */
    set_block_unused(grp->block_bitmap, freeblock);
    jfs_write_block(jfs, block, blockgroup * BLOCKS_PER_GROUP);
}

/* inode_to_block is a simple function that you pass the number of an
   inode, and it returns the number of the block in which that inode
   is stored */

int inode_to_block(int inode) 
{
    int blockgroup;
    int inode_in_block;
    int blocknum;
    if (inode == -1) {
	fprintf(stderr, "Internal error: bad inode %d\n", inode);
	exit(2);
    }
    
    /* which blockgroup is this inode in? */
    blockgroup = inode / INODES_PER_GROUP;

    /* what is the number of the inode within the block group */
    inode_in_block = inode % INODES_PER_GROUP;

    blocknum = (blockgroup * BLOCKS_PER_GROUP) /*where the blockgroup starts */
	+ 1 /* skip the superblock */
	+ inode_in_block / INODES_PER_BLOCK; /* which block this inode is in */

    return blocknum;
}

/* find_root_directory returns the inode number of the root directory */

int find_root_directory(jfs_t *jfs)
{
    char block[BLOCKSIZE];
    struct blockgroup *grp;
    
    /* read the superblock */
    jfs_read_block(jfs, block, 0);
    grp = (struct blockgroup*)block;
    return grp->superblock.first_inode;
}

/* get_inode reads a copy of the selected inode into the memory
   pointed at by i_node. */

void get_inode(jfs_t *jfs, int inodenum, struct inode *i_node) 
{
    char block[BLOCKSIZE];
    int offset;
    jfs_read_block(jfs, block, inode_to_block(inodenum));
    offset = (inodenum % INODES_PER_BLOCK) * INODE_SIZE;
    memcpy(i_node, block + offset, INODE_SIZE);
}


/* mk_new_directory creates a new directory on the "disk".  It
   automatically handles creating the "." and ".." directories within
   the new directory. */
/* the grandparent_inodenum is only used when creating .. directories */

int mk_new_directory(jfs_t *jfs, char *pathname, 
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
	new_inodenum = get_free_inode(jfs);
    }


    /* update the parent directories inode size field */
    jfs_read_block(jfs, block, inode_to_block(parent_inodenum));
    parent_inode = (struct inode*)(block + (parent_inodenum % INODES_PER_BLOCK)
				   * INODE_SIZE);

    prev_size = parent_inode->size;
    parent_inode->size += size;
    jfs_write_block(jfs, block, inode_to_block(parent_inodenum));

    /* create an entry in the parent directory */
    create_dirent(jfs, pathname, DT_DIRECTORY, parent_inode->blockptrs[0],
		  prev_size, size, new_inodenum);


    if (strcmp(pathname, ".")!=0 && strcmp(pathname, "..")!=0) {
	/* it's a real directory */

	/* get a block to hold the directory */
	new_blocknum = get_free_block(jfs);

	/* read in the block that contains our new inode */
	jfs_read_block(jfs, block, inode_to_block(new_inodenum));
	
	new_inode = (struct inode*)(block + (new_inodenum % INODES_PER_BLOCK)
				    * INODE_SIZE);
	new_inode->flags = FLAG_DIR;
	new_inode->blockptrs[0] = new_blocknum;
	new_inode->size = 0;
	
	/* write back the inode block */
	jfs_write_block(jfs, block, inode_to_block(new_inodenum));
	return new_inodenum;
    }

    return parent_inodenum;
}

/* findfile_recursive returns the inode of a file */

int findfile_recursive(jfs_t *jfs, char *pathname, int inodenum,
		       int file_type) 
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
		/* we've found the entry with the right name. */
		/* Check it actually is a file */
		if (dir_entry->file_type != file_type) {
		    return -1;
		}
		return dir_entry->inode;
	    }
	    /* More path to go - check this is a directory */
	    if (dir_entry->file_type != DT_DIRECTORY) {
		return -1;
	    }
	    /* remove the first part of the path before recursing */
	    p++;
	    strcpy(newpathname, p);
	    return findfile_recursive(jfs, newpathname, dir_entry->inode,
				      file_type);
	}
	    
	bytes_done += dir_entry->entry_len;
	dir_entry = (struct dirent*)(block + bytes_done);

	if (bytes_done >= dir_size) {
	    break;
	}
    }

    /* we didn't find the file */

    /* the user mistyped the path */
    return -1;
}

jfs_t *init_jfs(struct disk_image *d_img) {
    jfs_t * jfs;
    jfs = (jfs_t *)malloc(sizeof(jfs_t));
    jfs->d_img = d_img;
    jfs->cache_head = NULL;
    jfs->cache_tail = NULL;
    return jfs;
}

/* jfs_write_block buffers all writes.  All actual writes take
   place later during commit */

int jfs_write_block(jfs_t *jfs, char *block, 
		    unsigned int blocknum) {
    struct cached_block *c_block;

#ifdef DEBUG
    printf("write %d\n", blocknum);
#endif
    /* first we search the memory cache to see if we've already got
       this block cached */
    c_block = jfs->cache_head;
    while (c_block != NULL) {
	if (c_block->blocknum == blocknum) {
	    /* we found a cached copy of the block */
	    /* overwrite the old data */
#ifdef DEBUG
	    printf("overwriting cached copy\n");
#endif
	    memcpy(c_block->blockdata, block, BLOCKSIZE);
	    return 0;
	}
	c_block = c_block->next;
    }

#ifdef DEBUG
    printf("creating cache copy\n");
#endif
    /* it's not in the memory cache, so create a new cache entry */
    c_block = (struct cached_block*)malloc(sizeof(struct cached_block));
    c_block->blocknum = blocknum;
    c_block->next = NULL;
    if (jfs->cache_tail != NULL)
	jfs->cache_tail->next = c_block;
    memcpy(c_block->blockdata, block, BLOCKSIZE);
    if (jfs->cache_head == NULL)
	jfs->cache_head = c_block;
    jfs->cache_tail = c_block;
    return 0;
}


/* jfs_read_block reads a block from disk, but returns a buffered copy
   if there is one. */

int jfs_read_block(jfs_t *jfs, char *block, 
		   unsigned int blocknum) {

    struct cached_block *c_block;

#ifdef DEBUG
    printf("read %d\n", blocknum);
#endif
    /* first we search the memory cache to see if we've already got
       this block cached */
    c_block = jfs->cache_head;
    while (c_block != NULL) {
	if (c_block->blocknum == blocknum) {
	    /* we found a cached copy of the block */
#ifdef DEBUG
	    printf("found cached copy\n");
#endif
	    memcpy(block, c_block->blockdata, BLOCKSIZE);
	    return 0;
	}
	c_block = c_block->next;
    }
    
    /* it's not cached */
    read_block(jfs->d_img, block, blocknum);

    /* a real FS would cache the block we just read here to improve
       performance, but we only cache to ensure write consistency
       during the middle of a transaction when we haven't actually
       written to disk yet */

    return 0;
}

/* jfs_commit flushes the buffers to disk, first writing the logfile,
   then writing the actual data, then clearing the logfile */
int jfs_commit(jfs_t *jfs) {
    struct cached_block *c_block;
    int i = 0, root_inodenum, logfile_inodenum, logfile_missing = 0;
    int blocks_done;
    struct inode logfile_inode;
    char buf[BLOCKSIZE];
    struct commit_block* cb;
    char *crashval;
    
    crashval = getenv("CRASH_AFTER");
    if (crashval != NULL) {
	crash_after = atoi(crashval);
    }
    

    root_inodenum = find_root_directory(jfs);
    logfile_inodenum = findfile_recursive(jfs, ".log", root_inodenum, DT_FILE);
    if (logfile_inodenum < 0) {
	fprintf(stderr, "Missing logfile!\n");
	logfile_missing = 1;
    }

    if (logfile_missing == 0) {
	get_inode(jfs, logfile_inodenum, &logfile_inode);

	/* prepare the commit block, but don't write it yet */
	memset(buf, 0, BLOCKSIZE);
	cb = (struct commit_block*)buf;
	cb->magicnum = 0x89abcdef; /* just something very unlikely to
					  occur in real data */
	cb->uncommitted = 1;
	/* set unused blockpointers to -1 */
	for (i = 0; i < INODE_BLOCK_PTRS; i++)
	    cb->blocknums[i] = -1;
	cb->sum = 0;

	/* write the blocks to the logfile */
	c_block = jfs->cache_head;
	blocks_done = 0;
	while (c_block != NULL) {
/*#define DEBUG*/
  /* you can define DEBUG for debugging purposes */
#ifdef DEBUG
	    printf("flushing block %d to log %d\n", c_block->blocknum, 
		   blocks_done);
#endif
	    write_block(jfs->d_img, c_block->blockdata, 
			logfile_inode.blockptrs[blocks_done]);

	    /* For testing purposes; crash at a controlled point after
	       a preset number of writes */
	    crash_after--;
	    if (crash_after == 0) {
		fprintf(stderr, "Crashing on demand!\n");
		unmount_disk_image(jfs->d_img);
		exit(1);
	    }

	    cb->blocknums[blocks_done] = c_block->blocknum;
	    cb->sum += c_block->blocknum;
	    c_block = c_block->next;
	    blocks_done++;
	    if (blocks_done == INODE_BLOCK_PTRS) {
		/* not enough space in log - abort creating the logfile */
		break;
	    }
	}
	if (blocks_done < INODE_BLOCK_PTRS) {
	    /* there's space for the commit block */
	    write_block(jfs->d_img, buf, 
			logfile_inode.blockptrs[blocks_done]);

	    /* If this were a real disk, we'd validate this write
	       actually reached the disk by reading it back at this
	       point. */
	}
    }

    /* Now we've written all the data to the logfile.  Time to write
       it to where it's supposed to end up */

    c_block = jfs->cache_head;
    while (c_block != NULL) {
#ifdef DEBUG
	printf("flushing block %d\n", c_block->blocknum);
#endif
	write_block(jfs->d_img, c_block->blockdata, c_block->blocknum);

	/* For testing purposes; crash at a controlled point after
	   a preset number of writes */
	crash_after--;
	if (crash_after == 0) {
	    fprintf(stderr, "Crashing on demand!\n");
	    unmount_disk_image(jfs->d_img);
	    exit(1);
	}

	jfs->cache_head = c_block->next;
	free(c_block);
	c_block = jfs->cache_head;
    }
    jfs->cache_tail = NULL;

    /* If this were a real disk, we'd validate the last write actually
       reached the disk by reading it back at this point. */

    /* So now the data has reached the disk.  We can now erase the
       logfile by overwriting the previous commit block. */
    if (blocks_done < INODE_BLOCK_PTRS) {
	for (i = 0; i < INODE_BLOCK_PTRS; i++)
	    cb->blocknums[i] = -1;
	cb->sum = 0;
	cb->uncommitted = 0;
	write_block(jfs->d_img, buf, 
		    logfile_inode.blockptrs[blocks_done]);
    }

    return 0;
}

