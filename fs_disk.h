#define BLOCKSIZE 512  /* we'll use a blocksize of 512 bytes */

struct disk_image {
  char *pathname;
  int fd;  /* file descriptor */
  char *image_buf;  /* pointer to mounted buffer*/
  unsigned int size;  /* size of disk image */
};

/* prototypes */
void create_disk_image(char *filename, int size);
struct disk_image* mount_disk_image(char *filename);
void unmount_disk_image(struct disk_image *d_img);
int write_block(struct disk_image *d_img, char *block, unsigned int blocknum);
int read_block(struct disk_image *d_img, char *block, unsigned int blocknum);
