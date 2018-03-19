#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include "assert.h"
#include "console.h"
#include "task.h"
#include "vfs.h"
#include "pipe.h"
#include "serial.h"
#include "canonize.h"
#include "bname.h"
#include "dev.h"
#include "syscall.h"
#include "sysfile.h"

static struct file *file_alloc()
{
    struct file *f;
    f = calloc(1, sizeof(struct file));
    return f;
}

/**
 * Allocates a file descriptor pointer for current process.
 * If it does not find a free one, grow the files pointers buffer.
 * Returns a new file descriptor or -1 on error (MAX_OPEN_FILES reached).
 */
static int fd_alloc()
{
    int fd, n = current_task->num_files;

    for (fd = 0; fd < n; fd++) {
        if (!current_task->files[fd])
            break;
    }
    if (fd == n) {
        /* Not enough available files, grow the array. */
        if (fd > MAX_OPEN_FILES)
            return -1;
        n = n * 2 >= MAX_OPEN_FILES ? MAX_OPEN_FILES : n * 2;
        current_task->files = realloc(current_task->files,
                n * sizeof(*current_task->files));
        memset(current_task->files + current_task->num_files, 0,
                (n - current_task->num_files) * sizeof(*current_task->files));
        current_task->num_files = n;
    }
    return fd;
}

int sys_open(char *filename, int flags, int mode)
{
    // treat mode here!!! //
    int fd;
    struct file *file;
    if ((fd = fd_alloc()) < 0)
        return -1;
    if (!(file = file_alloc()))
        goto err;
    if ((fs_open_namei(filename, flags, mode, &file->fs_node)) < 0)
        goto err;
    if (!file->fs_node)
        goto err;

    if (flags & O_APPEND && (flags & O_WRONLY || flags & O_RDWR))
        file->offs = file->fs_node->size;
    else
        file->offs = 0;

    file->flags = flags;
    file->mode = mode;
    current_task->files[fd] = file;

    if (S_ISCHR(file->fs_node->type) || S_ISBLK(file->fs_node->type))
        dev_open(file->fs_node, flags);

    return fd;

err:
    if (file->fs_node)
        fs_close(file->fs_node);
    if (file)
        free(file);
    current_task->files[fd] = NULL;
    return -1;
}

/**
 * Checks if this file descriptor is valid
 */
static bool fd_is_valid(int fd)
{
    if (fd > current_task->num_files) {
        kprintf("pid: %d, fd %d is bigger than allocated: %d\n", 
                current_task->pid, fd, current_task->num_files);
        return false;
    }
    if (fd < 0) {
        kprintf("pid: %d, fd: %d; negative file number!\n",
                current_task->pid, fd);
        return false;
    }
    if (!current_task->files[fd]) {
        kprintf("File %d was not opened by proc %d\n", fd, current_task->pid);
        return false;
    }
    return true;
}

int sys_close(int fd)
{
    if (!fd_is_valid(fd))
        return -1;

    if (current_task->files[fd]->dup_cnt < 0) {
        kprintf("dup close error ?!\n");
        return -1;
    }
    fs_close(current_task->files[fd]->fs_node);
    /* When dup_cnt is 0, this struct file is no longer linked
     * inside process files, and it is safe to free it */
    if (current_task->files[fd]->dup_cnt == 0) {
        free(current_task->files[fd]);
    } else {
        current_task->files[fd]->dup_cnt--;
    }
    current_task->files[fd] = NULL;
    return 0;
}

static void populate_stat_buf(fs_node_t *fs_node, struct stat *buf)
{
    buf->st_dev = 0;					// the device id where it reside
    buf->st_ino = fs_node->inode;
    buf->st_size = fs_node->size;
    buf->st_mode = fs_node->type | fs_node->mask;
    buf->st_nlink = fs_node->num_links; // number of links
    buf->st_uid = fs_node->uid;
    buf->st_gid = fs_node->gid;
    buf->st_rdev = makedev(fs_node->major, fs_node->minor);
    buf->st_atime = fs_node->atime;
    buf->st_mtime = buf->st_mtime;
    buf->st_ctime = buf->st_ctime;
}

// fixme me links //
int sys_stat(char *path, struct stat *buf)
{
    fs_node_t *fs_node;
    if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
        return -1;
    }
    populate_stat_buf(fs_node, buf);
    fs_close(fs_node);
    return 0;
}

// fix me links //
int sys_lstat(char *path, struct stat *buf) 
{
    fs_node_t *fs_node;
    if(fs_open_namei(path, O_RDONLY, 0755, &fs_node) < 0) {
        return -1;
    }
    populate_stat_buf(fs_node, buf);
    fs_close(fs_node);
    return 0;
}

int sys_fstat(int fd, struct stat *buf)
{
    struct file *f;
    if (!fd_is_valid(fd))
        return -1;
    f = current_task->files[fd];
    populate_stat_buf(f->fs_node, buf);
    return 0;
}

int sys_read(int fd, void *buf, size_t count)
{
    struct file *f;
    unsigned int bytes;
    
    if (!fd_is_valid(fd))
        return -1;
    
    f = current_task->files[fd];
    
    if(f->offs >= f->fs_node->size
            && f->fs_node->type != FS_CHARDEVICE 
            && f->fs_node->type != FS_PIPE) {
        return 0; // at EOF
    }
    bytes = fs_read(f->fs_node, f->offs, count, buf);
    f->offs += bytes;
    return bytes;
}

int sys_write(int fd, void *buf, size_t count)
{
    struct file *f;
    unsigned int bytes;

    if (!fd_is_valid(fd))
        return -1;

    f = current_task->files[fd];
    
    bytes = fs_write(f->fs_node, f->offs, count, buf);
    f->offs += bytes;
    return bytes;
}

// alias //
int write(int fd, void *buf, size_t count)
{
    return sys_write(fd, buf, count);
}

int sys_chdir(char *path)
{
    char *p;
    p = canonize_path(current_task->cwd, path);
    fs_node_t *fs_node;
    if ((fs_open_namei(p, O_RDONLY, 0777, &fs_node)) < 0) {
        kprintf("sys_chdir() cannot open %s\n", path);
        free(p);
        return -1;
    }
    if (!(fs_node->type & FS_DIRECTORY)) {
        kprintf("sys_chdir() - %s is not a directory\n", path);
        free(p);
        fs_close(fs_node);
        return -1;
    }
    free(current_task->cwd);
    current_task->cwd = p;
    fs_close(fs_node);
    return 0;
}

int sys_chroot(char *path)
{
    fs_node_t *fs_node;
    if ((fs_open_namei(path, O_RDONLY, 0777, &fs_node)) < 0) {
        return -1;
    }
    if (!(fs_node->type & FS_DIRECTORY)) {
        fs_close(fs_node);
        return -1;
    }
    // TODO - also check permissions //
    current_task->root_dir = fs_node;
    return 0;
}

// TODO: check permissions
int sys_chown(char *filename, int uid, int gid)
{
    fs_node_t *fs_node;
    if ((fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) < 0) {
        return -1;
    }
    fs_node->uid = uid;
    fs_node->gid = gid;
    fs_close(fs_node);
    return 0;
}

int sys_chmod(char *filename, int mode)
{
    fs_node_t *fs_node;
    if ((fs_open_namei(filename, O_RDONLY, 0777, &fs_node)) < 0) {
        return -1;
    }
    fs_node->mask = mode;
    fs_close(fs_node);
    return 0;
}

int sys_mkdir(char *path, int mode)
{
    int i, len;
    char *tmp, *dirname, *basename;
    if(!path)
        return -1;
    if (*path != '/') { // relative path
        tmp = calloc(1, strlen(current_task->cwd) + strlen(path) + 4);
        strcat(tmp, current_task->cwd);
        strcat(tmp, "/");
        strcat(tmp, path);
    } else {			// absolute path
        tmp = strdup(path);
    }

    //tmp = canonize_path(current_task->cwd, path);
    // FIXME: use canonize_path
    while (tmp && tmp[strlen(tmp) - 1] == '/') 
        tmp[strlen(tmp) - 1] = 0;
    len = strlen(tmp);
    for (i = len; i > 0; i--)
        if (tmp[i] == '/')
            break;
    if(i == len)
        return -1;

    dirname = strdup(tmp); 
    dirname[i] = 0;
    basename = strdup(tmp); 
    memmove(basename, basename+i+1, len-i); basename[len-i]=0;
    fs_node_t *dir;

    if (fs_open_namei(dirname, O_RDONLY, 0777, &dir)<0) {
        return -1;
    }
    fs_node_t *file;
    if ((file = fs_finddir(dir, basename))) {
        fs_close(dir);
        fs_close(file);
        return -1;
    }
    if (!(file = fs_mkdir(dir, basename, mode))) {
        fs_close(dir);
        return -1;
    }
    fs_close(file);
    fs_close(dir);
    free(dirname);
    free(basename);
    free(tmp);
    return 0;
}

int sys_isatty(int fd)
{
    struct file *f;
    if (!fd_is_valid(fd))
        return 0;
    f = current_task->files[fd];
    if (f->fs_node && FS_CHARDEVICE) {
        return 1;
    }
    return 0;
}

off_t sys_lseek(int fd, off_t offset, int whence)
{
    struct file *f;
    if (!fd_is_valid(fd))
        return -1;
    f = current_task->files[fd];
    if (whence == SEEK_SET) {
        if (offset > f->fs_node->size) {
            kprintf("sys_lseek() offs > size\n");
            return -1;
        }
        f->offs = offset;
    } else if (whence == SEEK_CUR) {
        if (offset + f->offs > f->fs_node->size) {
            kprintf("sys_lseek() offs + f->offs > fs_node->size\n");
            return -1;
        }
        f->offs += offset;
    } else if (whence == SEEK_END) {
        f->offs = offset;
    }
    return f->offs;
}

DIR *sys_opendir(char *dirname)
{
    DIR *d;
    int fd = sys_open(dirname, O_RDONLY, 0);
    if(!fd) {
        return NULL;
    }
    d = current_task->files[fd];
    if(!(d->fs_node->type & FS_DIRECTORY)) {
        sys_close(fd);
        return NULL;
    }
    return d;
}

int sys_closedir(DIR *dir)
{
    int fd;
    for (fd = 0; fd < current_task->num_files; fd++) {
        if (dir == current_task->files[fd]) {
            sys_close(fd);
            return 0;
        }
    }
    return -1;
}

// i cannot return in user mode a buffer from kernel mode
// that's why i will pass buf from user mode and populate in k mode
int sys_readdir(DIR *dir, struct dirent *buf)
{
    struct dirent *d;
    if (!(d = fs_readdir(dir->fs_node, dir->offs))) {
        return -1;
    }
    strncpy(buf->d_name, d->d_name, sizeof(buf->d_name) - 1);
    buf->d_ino = d->d_ino;
    dir->offs++;
    return 0;
}

int sys_readlink(const char *pathname, char *buf, size_t bufsiz) {
    (void) pathname;
    (void) buf;
    (void) bufsiz;
    kprintf("unimplemented\n");
    return -1;
}

/**
 * TODO: implement these
 * int readdir_r(DIR *, struct dirent *, struct dirent **);
 * void rewinddir(DIR *);
 * void seekdir(DIR *, long int);
 * long int telldir(DIR *);
 */

char *sys_getcwd(char *buf, size_t size) 
{
    unsigned int len = strlen(current_task->cwd);
    if (len + 1 > size) {
        kprintf("len: %d, size %d\n", len, size);
        return NULL;
    }
    strncpy(buf, current_task->cwd, len);
    buf[len]=0;
    return buf;
}

int sys_unlink(char *path) 
{
    fs_node_t *node;
    if ((fs_open_namei(path, O_RDONLY, 0, &node)) < 0) {
        return -1;
    }
    fs_close(node);
    return fs_unlink(path);
}

int sys_dup(int oldfd) 
{
    int fd;
    bool valid = false;
    for (fd = 0; fd < current_task->num_files; fd++) {
        if (fd == oldfd && current_task->files[fd]) {
            valid = true; 
            break;
        }
    }
    if(!valid) {
        kprintf("sys_dup: invalid file descriptor %d!", oldfd);
        return -1;
    }	
    if ((fd = fd_alloc()) < 0) {
        return -1;
    }
    // and we link the files[oldfd] to new files[fd]
    // increasing dup_cnt
    current_task->files[fd] = current_task->files[oldfd];
    fs_dup(current_task->files[fd]->fs_node);
    current_task->files[fd]->dup_cnt++;
    return fd;
}

int sys_pipe(int fd[2])
{
    int in, out;
    struct file *fin, *fout;
    fs_node_t *nodes[2];
    pipe_new(nodes);
    fin = file_alloc();
    fout = file_alloc();
    fin->flags = O_RDONLY;
    fin->fs_node = nodes[0];
    fout->flags = O_WRONLY;
    fout->fs_node = nodes[1];
    out = fd_alloc();
    fd[0] = in = fd_alloc();
    current_task->files[in] = fin;
    fd[1] = out = fd_alloc();
    current_task->files[out] = fout;
    nodes[0]->open(nodes[0], O_RDONLY);
    nodes[1]->open(nodes[1], O_WRONLY);
    return 0;
}

int sys_link(char *oldpath, char *newpath) 
{	
    char *old, *new, 
         old_dir[MAX_PATH_LEN],
         new_dir[MAX_PATH_LEN], 
         file[MAX_PATH_LEN];
    fs_node_t *node, *parent;
    int ret;

    old = canonize_path(current_task->cwd, oldpath);
    new = canonize_path(current_task->cwd, newpath);

    if (fs_open_namei(new, O_RDONLY, 0, &node) == 0) {
        if (!(node->type & FS_DIRECTORY)) {
            kprintf("sys_link: dest path %s already exist\n", newpath);
            fs_close(node);
            free(old); 
            free(new); 
            return -1;
        } else { // dest is a directory, we will link into it //
            parent = node;
        }
    }
    if (fs_open_namei(old, O_RDWR, 0, &node) < 0) {
        kprintf("sys_link: path %s does not exist\n", oldpath);
        free(old); 
        free(new);
        return -1;
    }
    if (!parent) {
        bname(newpath, new_dir, file);
        if (fs_open_namei(new_dir, O_RDWR, 0, &parent) < 0) {
            kprintf("sys_link: destination dir %s does not exist\n", new_dir);
            fs_close(node);
            free(old); 
            free(new);
            return -1;
        }
        if (!(parent->type & FS_DIRECTORY)) {
            kprintf("sys_link: dest %s is not a directory\n", new_dir);
            fs_close(parent);fs_close(node);
            free(old); free(new);
            return -1;
        }
    }
    // if newpath does not contain a filename == is a directory, 
    // get file name from oldpath //
    if (file[0] == 0) {
        bname(oldpath, old_dir, file);	
    }
    ret = fs_link(parent, node, file);
    fs_close(parent);
    fs_close(node);
    free(old);
    free(new);
    return ret;
}

// do some checkings and call fs_rename which calls file system specific 
// implementation
int sys_rename(char *oldpath, char *newpath) 
{
    char *old, *new,
         dir[MAX_PATH_LEN],
         file[MAX_PATH_LEN];

    fs_node_t *node, *parent, *tnode = NULL;
    int ret = -1;
    int inode;

    old = canonize_path(current_task->cwd, oldpath);
    new = canonize_path(current_task->cwd, newpath);
    if (strcmp(old, new) == 0) {
        goto cln1;	
    }
    bname(new, dir, file);
    if (fs_open_namei(old, O_RDWR, 0, &node) < 0) {
        kprintf("sys_rename: cannot open oldpath %s\n", oldpath);
        goto cln1;
    }
    if (fs_open_namei(dir, O_RDWR, 0, &parent) < 0) {
        kprintf("sys_rename: cannot open dest dir %s\n", dir);
        goto cln2;
    }
    if (fs_open_namei(new, O_RDWR, 0, &tnode) == 0) {
        inode = tnode->inode;
        fs_unlink(new);
    }
    ret = fs_link(parent, node, file);
    if (ret < 0) {
        kprintf("Lost inode %d! trying to repair\n", inode);
        // lost both nodes!! //
        if (fs_link(parent, tnode, tnode->name) == 0) {
            kprintf("repaired inode %d\n", inode);
        } else {
            kprintf("inode %d lost\n", inode);
        }
        goto cln2;
    } else {
        fs_unlink(old);
    }

    if (tnode)
        fs_close(tnode);
    fs_close(parent);
cln2:
    fs_close(node);
cln1:
    free(old);
    free(new);
    return ret;
}

int sys_ioctl(int fd, int request, void *argp)
{
    struct file *f;

    validate_usr_ptr(argp);
    if (!fd_is_valid(fd))
        return 0;
    f = current_task->files[fd];

    return fs_ioctl(f->fs_node, request, argp);
}

int sys_ftruncate(int fd, off_t len)
{
    struct file *f;

    if (!fd_is_valid(fd))
        return -1;

    f = current_task->files[fd];

    return fs_truncate(f->fs_node, len);
}

