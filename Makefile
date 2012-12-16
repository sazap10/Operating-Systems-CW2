all::	jfs_format jfs_ls jfs_mkdir jfs_fsck jfs_copyin jfs_copyout jfs_rm jfs_checklog
clean:	
	rm -f *.o *~ jfs_format jfs_ls jfs_mkdir jfs_fsck jfs_copyin jfs_copyout jfs_rm jfs_checklog

COMMON_FILES = jfs_common.o fs_disk.o
jfs_format:	jfs_format.o	$(COMMON_FILES)
jfs_ls:		jfs_ls.o	jfs_common.o fs_disk.o
jfs_mkdir:	jfs_mkdir.o	jfs_common.o fs_disk.o
jfs_fsck:	jfs_fsck.o	jfs_common.o fs_disk.o
jfs_copyin:	jfs_copyin.o	jfs_common.o fs_disk.o
jfs_copyout:	jfs_copyout.o 	jfs_common.o fs_disk.o
jfs_rm:		jfs_rm.o	jfs_common.o fs_disk.o
jfs_checklog:	jfs_checklog.o	jfs_common.o fs_disk.o

CC = gcc
CFLAGS = -g -Wall

.c.o:	
	$(CC) $(CFLAGS) -c -o $@ $<
