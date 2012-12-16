#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>

#define BLOCKSIZE 512  /* we'll use a blocksize of 512 bytes */

struct disk_image {
  char *pathname;
  int fd;  /* file descriptor */
  char *image_buf;  /* pointer to mounted buffer*/
  unsigned int size;  /* size of disk image */
};

void create_disk_image(char *filename, int size) 
{
  struct stat statbuf;
  int fd;
  char *image_buf;
  char pathname[MAXPATHLEN+1];

  /* If filename isn't an absolute pathname, then we'd better prepend
     the current working directory to it */
  if (filename[0] == '/') {
    strncpy(pathname, filename, MAXPATHLEN);
  } else {
    getcwd(pathname, MAXPATHLEN);
    if (strlen(pathname) + strlen(filename) + 1 > MAXPATHLEN) {
      fprintf(stderr, "Filename too long\n");
      exit(1);
    }
    strcat(pathname, "/");
    strcat(pathname, filename);
  }
    
  /* Step 1: we want to check that this isn't going to overwrite
     another file */
  /* we can use "stat" to do this, by checking the file status */
  if (stat(pathname, &statbuf) >= 0) {
    fprintf(stderr, "Cannot create disk image file %s: file already exists\n", 
	    pathname);
    exit(1);
  }
  
  /* Step 2: we create the file */

  /* open the file */
  fd = open(pathname, O_RDWR | O_CREAT);
  if (fd < 0) {
    fprintf(stderr, "Cannot create disk image file %s:\n%s\n",
	    pathname, strerror(errno));
    exit(1);
  }

  /* fix the file permissions */
  fchmod(fd, S_IRUSR | S_IWUSR);

  /* Step 3: write a lot of garbage to the file */
  /* we set all the bytes in the mmapped file to 0xff */
  /* later on this will help us debug, because we know that all
     unallocated blocks should be filled with this */
  image_buf = malloc(size);
  memset(image_buf, 0xff, size);
  write(fd, image_buf, size);
  
  /* Step 4: close the file  and clean up*/
  close(fd);
  free(image_buf);
}


struct disk_image* mount_disk_image(char *filename)
{
  struct stat statbuf;
  int fd, size;
  char *image_buf;
  char pathname[MAXPATHLEN+1];
  struct disk_image *d_img;

  /* If filename isn't an absolute pathname, then we'd better prepend
     the current working directory to it */
  if (filename[0] == '/') {
    strncpy(pathname, filename, MAXPATHLEN);
  } else {
    getcwd(pathname, MAXPATHLEN);
    if (strlen(pathname) + strlen(filename) + 1 > MAXPATHLEN) {
      fprintf(stderr, "Filename too long\n");
      exit(1);
    }
    strcat(pathname, "/");
    strcat(pathname, filename);
  }

  /* Step 2: find out how big the disk image file is */
  /* we can use "stat" to do this, by checking the file status */
  if (stat(pathname, &statbuf) < 0) {
    fprintf(stderr, "Cannot read disk image file %s:\n%s\n", 
	    pathname, strerror(errno));
    exit(1);
  }

  size = statbuf.st_size;

  /* Step 3: open the file for read/write */
  fd = open(pathname, O_RDWR);
  if (fd < 0) {
    fprintf(stderr, "Cannot read disk image file %s:\n%s\n", 
	    pathname, strerror(errno));
    exit(1);
  }

  /* Step 3: we memory map the file */

  image_buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (image_buf == MAP_FAILED) {
    fprintf(stderr, "Failed to memory map: \n%s\n", strerror(errno));
    exit(1);
  }

  /* return the results */

  d_img = malloc(sizeof(struct disk_image));
  d_img->pathname = strdup(pathname);
  d_img->size = size;
  d_img->image_buf = image_buf;
  d_img->fd = fd;
  
  return d_img;
}

void unmount_disk_image(struct disk_image *d_img)
{
  /* sync the memory mapped buffer to disk */
  if (msync(d_img->image_buf, d_img->size, MS_SYNC) < 0) {
    perror("msync failed");
  }

  /* un-memory-map the disk image */
  if (munmap(d_img->image_buf, d_img->size) < 0) {
    perror("munmap failed");
  }

  /* close the file */
  close(d_img->fd);

  /* free the disk image struct */
  free(d_img->pathname);
  free(d_img);
}

int write_block(struct disk_image *d_img, char *block, unsigned int blocknum) {
  if (d_img->image_buf == NULL) {
    fprintf(stderr, "write_block: bad disk image file\n");
    return -1;
  }
  if (blocknum+1 * BLOCKSIZE > d_img->size) {
    fprintf(stderr, "write_block: blocknum %d is too large\n", blocknum);
    abort();
    return -1;
  }

  /* printf("write block number %d\n", blocknum); */
  memcpy(d_img->image_buf + (BLOCKSIZE * blocknum), block, BLOCKSIZE);
  return 0;
}

int read_block(struct disk_image *d_img, char *block, unsigned int blocknum) {
  if (d_img->image_buf == NULL) {
    fprintf(stderr, "read_block: bad disk image file\n");
    return -1;
  }
  if (blocknum+1 * BLOCKSIZE > d_img->size) {
    fprintf(stderr, "read_block: blocknum %d is too large\n", blocknum);
    return -1;
  }

  memcpy(block, d_img->image_buf + (BLOCKSIZE * blocknum), BLOCKSIZE);
  return 0;
}

