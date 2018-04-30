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
// #include <kern/fcntl.h>

typedef long long off_t;

// #define O_RDONLY 0 /* Open for read */
// #define O_WRONLY 1 /* Open for write */
// #define O_RDWR 2   /* Open for read and write */

enum flag_type {
    O_RDONLY_FLAG,
    O_WRONLY_FLAG,
    O_RDWR_FLAG,
};

struct file_table {
    off_t offset;
    enum flag_type flag;
    int ref_count;
    struct vnode* file;
};

/*
 * Put your function declarations and data types here ...
 */
int sys_open(const_userptr_t filename, int flags, mode_t mode, int *retval);
int sys_close(int fd, int *retval);
int sys_read(int fd, void *buf, size_t buflen, int *retval);
int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval);
int lseek(int fd, off_t pos, int whence);
int sys_dup2(int oldfd, int newfd, int *retval);
void uio_uinit(struct iovec *iov, struct uio *u, userptr_t buf,
               size_t len, off_t offset, enum uio_rw rw);

#endif /* _FILE_H_ */
