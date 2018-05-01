#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>
#include <proc.h>
#include <synch.h>
// static struct lock *file_lock;

/*
 * Add your file-related functions here ...
 */

int sys_open(const_userptr_t filename, int flags, mode_t mode, int *retval)
{
    
    // process file table is full
    if (curproc->left_number == 0) {
        return EMFILE;
    }
    // check if filename is valid
    int res;
    char *name = kmalloc(__PATH_MAX * sizeof(char));
    size_t actual;
    res = copyinstr(filename, name, __PATH_MAX, &actual);
    if (res) {
        return res;
    }
    kprintf("OPEN: open path is %s\n", name);

    // init vnode and create file pointer in process file table
    struct vnode* v;
    int i = 3;
    res = vfs_open(name, flags, mode, &v);
    if (res) {
        return res;
    }
    struct file_table *ft = kmalloc(sizeof(struct file_table));
    ft->offset = 0;
    ft->flag = flags;
    ft->ref_count = 1;
    ft->file = v;
    ft->file_lock_refcount = lock_create("file_lock_refcount");
    ft->file_lock = lock_create("file_lock");      // init lock
    if (ft->file_lock == NULL || ft->file_lock_refcount == NULL){
        return EFAULT;
    }
    while (curproc->p_file[i] != NULL) {
        i++;
    }
    curproc->p_file[i] = ft;
    curproc->left_number--;
    *retval = i;
    kfree(name);
    kprintf("OPEN: return fd is %d\n", *retval);
    return 0;
}


int sys_close(int fd, int *retval) {
    if (!curproc->p_file[fd]) {
        *retval = EBADF;
        return -1;
    }
    int ref_count = curproc->p_file[fd]->ref_count;
    if (ref_count == 1) {
        vfs_close(curproc->p_file[fd]->file);
        lock_destroy(curproc->p_file[fd]->file_lock_refcount);
        lock_destroy(curproc->p_file[fd]->file_lock);    //free lock
        kfree(curproc->p_file[fd]);
        curproc->p_file[fd] = NULL;
    } else {
        lock_acquire(curproc->p_file[fd]->file_lock_refcount);
        curproc->p_file[fd]->ref_count = ref_count - 1;
        lock_release(curproc->p_file[fd]->file_lock_refcount);

    }
    curproc->left_number++;
    return 0;
}


void uio_uinit(struct iovec *iov, struct uio *u, userptr_t buf,
          size_t len, off_t offset, enum uio_rw rw)
{
    iov->iov_ubase = buf;
    iov->iov_len = len;
    u->uio_iov = iov;
    u->uio_iovcnt = 1;
    u->uio_offset = offset;
    u->uio_resid = len;
    u->uio_segflg = UIO_USERSPACE;
    u->uio_rw = rw;
    u->uio_space = curproc->p_addrspace;
}

int sys_read(int fd, void *buf, size_t buflen, int *retval) {
    //check if file exits and authority 
    if (curproc->p_file[fd] == NULL || curproc->p_file[fd]->flag == 1) {
        return EBADF;
    }
    int res;
    // check if pointer is valid
    void *dest = kmalloc(buflen);
    res = copyin(buf, dest, buflen);
    if (res) {
        kfree(dest);
        return res;
    }
    kfree(dest);
    lock_acquire(curproc->p_file[fd]->file_lock); // lock
    // start writing
    struct iovec iov;
    struct uio u;
    off_t offset = curproc->p_file[fd]->offset;
    kprintf("READ: read fd is %d\n", fd);
    kprintf("READ: begin offset is %lld\n", offset);
    struct vnode *vur_v = curproc->p_file[fd]->file;
    uio_uinit(&iov, &u, buf, buflen, offset, UIO_READ);

    res = VOP_READ(vur_v, &u);
    if (res) {
        kprintf("read res is %d\n", res);
        lock_release(curproc->p_file[fd]->file_lock); //lock
        return res;
    }
    curproc->p_file[fd]->offset = u.uio_offset;
    *retval = u.uio_offset - offset;
    lock_release(curproc->p_file[fd]->file_lock); //lock

    kprintf("READ: read length is %d\n", *retval);
    kprintf("READ: end offset is %lld\n", curproc->p_file[fd]->offset);
    kprintf("READ: test length is %d\n", buflen - u.uio_resid);        
    return 0;
}



int sys_write(int fd, userptr_t buf, size_t nbytes, int *retval) {
    //check if file exits and authority 
    if (curproc->p_file[fd] == NULL || curproc->p_file[fd]->flag == 0) {
        return EBADF;
    }
    // check buffer pointer
    int res;
    void *dest = kmalloc(nbytes);
    res = copyin(buf, dest, nbytes);
    if (res) {
        kprintf("error: %d, fd is %d\n", res, fd);
        kfree(dest);
        return res;
    }
    kfree(dest);
    lock_acquire(curproc->p_file[fd]->file_lock);  //lock
    // start writing
    struct iovec iov;
    struct uio u;
    off_t offset = curproc->p_file[fd]->offset;
    struct vnode *vur_v = curproc->p_file[fd]->file;
    if (fd > 2) {
        kprintf("WRITE: write fd is %d\n", fd);
        kprintf("WRITE: begin offset is %lld\n", offset);
    }
    uio_uinit(&iov, &u, buf, nbytes, offset, UIO_WRITE);
    res = VOP_WRITE(vur_v, &u);
    if (res) {
        kprintf("vop_write res is %d\n", res);
        lock_release(curproc->p_file[fd]->file_lock);  //lock
        return res;
    }
    curproc->p_file[fd]->offset = u.uio_offset;
    *retval = u.uio_offset - offset;
    lock_release(curproc->p_file[fd]->file_lock);     //lock
    // kprintf("offset is %lld\n", curproc->p_file[fd]->offset);
    if (fd > 2) {
        kprintf("WRITE: write length is %d\n", *retval);
        kprintf("WRITE: end offset is %lld\n", curproc->p_file[fd]->offset);
    }

    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval) {
    // check file descriptor 
    if (curproc->p_file[oldfd] == NULL || curproc->p_file[newfd] != NULL) {
        return EBADF;
    }
    if (!curproc->left_number) {
        return EMFILE;
    }
    // assign old process file pointer to the new one
    struct file_table *ft = curproc->p_file[oldfd];
    curproc->p_file[newfd] = ft;
    lock_acquire(curproc->p_file[newfd]->file_lock_refcount);
    ft->ref_count++;
    *retval = newfd;
    lock_release(curproc->p_file[newfd]->file_lock_refcount);
    return 0;
}

int lseek(int fd, off_t pos, int whence, off_t *retval) {
    if (curproc->p_file[fd] == NULL) {
        return EBADF;
    }
    kprintf("LSEEK: test lseek fd is %d\n", fd);
    int result;
    result = VOP_ISSEEKABLE(curproc->p_file[fd]->file);
    if (!result) {
        kprintf("LSEEK: %d does not support lseek\n", fd);
        return ESPIPE;
    }
    lock_acquire(curproc->p_file[fd]->file_lock);
    if (whence == SEEK_SET) {
        curproc->p_file[fd]->offset = pos;
    } else if (whence == SEEK_CUR) {
        curproc->p_file[fd]->offset += pos;
    } else if (whence == SEEK_END) {
        struct stat statbuf;
        result = VOP_STAT(curproc->p_file[fd]->file, &statbuf);
        curproc->p_file[fd]->offset = statbuf.st_size + pos;
    } else {
        lock_release(curproc->p_file[fd]->file_lock);
        return EINVAL;
    }
    *retval = curproc->p_file[fd]->offset;
    kprintf("LSEEK: lseek pos is %lld\n", pos);
    lock_release(curproc->p_file[fd]->file_lock);
    kprintf("LSEEK: lseek whence is %d\n", whence);
    kprintf("LSEEK: result is %lld\n", *retval);
    return 0;
}
