#define MAX_FILENAME_LEN 255

/* Our inodes will be 64 bytes long, and we use 8 bytes to hold the
   size and the flags, so that leaves 14 direct block we can
   address */
#define INODE_SIZE 64
#define INODE_BLOCK_PTRS ((INODE_SIZE - 8)/4)

/* An i-node holds the metadata about a file, and a list of the blocks
   that make up the file.  We'll use a very simple i-node, as we don't
   care about permissions and so on for this application */

#define FLAG_DIR 1  /* is this inode a directory? */

struct inode {
  unsigned int size;
  unsigned int flags;
  unsigned int blockptrs[INODE_BLOCK_PTRS];
};

/* a dirent (directory entry) holds information about a file */

#define DT_UNKNOWN 0
#define DT_DIRECTORY 1
#define DT_FILE 2

struct dirent {
  unsigned int file_type; /* the file's type */
  unsigned int entry_len; /* the total length of this dirent,
			     including the variable length name, and
			     any empty space following */
  unsigned int inode; /* the file's inode */
  unsigned int namelen;  /* how long the filename is */
  char name[4]; /* placeholder for filename - filename may actually
		   be longer than this */
};

/* we have one superblock in each blockgroup */

struct superblock_s {
    int size; /* the number of blocks in the filesystem */
    int free_blocks; /* the number of free blocks in the filesystem */
    int free_inodes; /* the number of free inodes in the filesystem */
    int first_inode; /* the block number of the first inode in the
			filesystem - this will contain the directory
			entry for "/" */
};

#define BLOCKS_PER_GROUP 256
#define INODES_PER_GROUP 64  
#define INODES_PER_BLOCK (BLOCKSIZE/INODE_SIZE)

/* Each block is 512 bytes.  Each i-node is 64 bytes.  Thus a block
   can hold 8 i-nodes, which means they occupy 8 blocks per
   blockgroup.  The superblock and bitmaps take one block. So that
   leaves 247 blocks for data in each blockgroup */

/* how far into the blockgroup the first data block is */
#define DATA_BLOCK_OFFSET (1 + INODES_PER_GROUP / INODES_PER_BLOCK)

struct blockgroup {
  struct superblock_s superblock;
  unsigned char block_bitmap[BLOCKS_PER_GROUP/8];
  unsigned char inode_bitmap[INODES_PER_GROUP/8];
};


/* data structures for the JFS memory cache */

typedef struct jfs_s {
    struct disk_image *d_img;
    struct cached_block *cache_head;
    struct cached_block *cache_tail;
} jfs_t; 

/* these form a linked list for simplicity.  A real FS would use a
   hash table to lookup the right block */

struct cached_block {
    unsigned int blocknum;
    char blockdata[BLOCKSIZE];
    struct cached_block* next;
};

struct commit_block {
    unsigned int magicnum; /* magic number to make it easy to find the
			      commit block */
    unsigned int uncommitted; /* 1 if the log has yet to be copied to disk,
				 0 if the log has been successfully copied. */
    int blocknums[INODE_BLOCK_PTRS]; /* where the blocks in the
					logfile should really be on
					the disk */
    unsigned int sum; /* The sum of the blocknums, as a sanity check.  A
			 real filesystem would use a proper
			 checksum. */
};

/* How big is the logfile?  As large as the largest file we can hold,
   so not too big actually.  Really should use indirect blocks here. */
#define LOGFILE_BLOCKS INODE_BLOCK_PTRS

/* prototypes for common functions */
void first_part();
void last_part();
void all_but_last_part();
int get_dirent_len();
void create_dirent();
int count_free_blocks(unsigned char *block_bitmap);
int get_free_block(jfs_t *jfs);
int find_free_block(unsigned char *block_bitmap);
void set_block_used(unsigned char *block_bitmap, int blocknum);
void return_block_to_freelist(jfs_t *jfs, int blocknum);
int get_free_inode(jfs_t *jfs);
int find_free_inode(unsigned char *inode_bitmap);
void set_inode_used(unsigned char *inode_bitmap, int inodenum);
void return_inode_to_freelist(jfs_t *jfs, int inodenum);
int inode_to_block(int inode);
int mk_new_directory();
int findfile_recursive(jfs_t *jfs, char *pathname, int inodenum,
		       int file_type);
int find_root_directory(jfs_t *jfs);
void get_inode(jfs_t *jfs, int inodenum, struct inode *i_node);

jfs_t *init_jfs(struct disk_image *d_img);
int jfs_write_block(jfs_t *jfs, char *block, 
		    unsigned int blocknum);
int jfs_read_block(jfs_t *jfs, char *block, 
		   unsigned int blocknum);
int jfs_commit(jfs_t *jfs);
