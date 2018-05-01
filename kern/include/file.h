/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#include <kern/unistd.h>
#include <types.h>
#include <uio.h>
#include <synch.h>
// #include <kern/fcntl.h>

typedef long long off_t;


enum flag_type {
    O_RDONLY_FLAG,   /* Open for read */
    O_WRONLY_FLAG,   /* Open for write */
    O_RDWR_FLAG,     /* Open for read and write */
};

struct file_table {
    off_t offset;
    enum flag_type flag;
    int ref_count;
    struct vnode* file;
    struct lock* file_lock;
    struct lock* file_lock_refcount;
};

/*
 * Put your function declarations and data types here ...
 */
int sys_open(const_userptr_t filename, int flags, mode_t mode, int *retval);
int sys_close(int fd, int *retval);
int sys_read(int fd, void *buf, size_t buflen, int *retval);
int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval);
int lseek(int fd, off_t pos, int whence, off_t *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
void uio_uinit(struct iovec *iov, struct uio *u, userptr_t buf,
               size_t len, off_t offset, enum uio_rw rw);

#endif /* _FILE_H_ */
