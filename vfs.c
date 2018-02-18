#include <string.h>
#include <stdlib.h>	// malloc
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include "console.h"
#include "serial.h"
#include "assert.h"
#include "task.h"
#include "vfs.h"
#include "canonize.h"
#include "bname.h"

void fs_open(fs_node_t *node, unsigned int flags)
{
    if (node->open != NULL) {
        return node->open(node, flags);
    }
}

void fs_close(fs_node_t *node)
{
    if (node == NULL) {
        panic("fs_close\n");
    }
    if (node->close) {
        return node->close(node);
    }
}

unsigned int fs_read(fs_node_t *node, unsigned int offset, 
                     unsigned int size, char *buffer)
{
    return node->read != NULL ? node->read(node, offset, size, buffer) : 0;
}

unsigned int fs_write(fs_node_t *node, unsigned int offset, 
                      unsigned int size, char *buffer)
{
    return node->write != NULL ? node->write(node, offset, size, buffer) : 0;
}


struct dirent *fs_readdir(fs_node_t *node, unsigned int index)
{
    if ((node->type & FS_DIRECTORY) && node->readdir != 0)
        return node->readdir(node, index);
    else
        return NULL;
}

fs_node_t *fs_finddir(fs_node_t *node, char *name)
{
    if ((node->type & FS_DIRECTORY) && node->finddir != 0)
        return node->finddir(node, name);
    else
        return NULL;
}

fs_node_t *fs_mkdir(fs_node_t *node, char *name, int mode)
{
    if(!(node->type & FS_DIRECTORY)) {
        return NULL;
    }
    if(fs_finddir(node, name)) {
        return NULL;
    }
    if(!node->mkdir) {
        return NULL;
    }
    return node->mkdir(node, name, mode);
}

fs_node_t *fs_dup(fs_node_t *node)
{
    spin_lock(&node->lock);
    node->ref_count++;
    spin_unlock(&node->lock);
    return node;
}

/**
 * Simple name to vfs inode find ~ namei
 */
fs_node_t *fs_namei(char *path)
{
    fs_node_t *node = NULL;
    KASSERT(*path);
    if (*path != '/') {
        panic("Path [%s] is not full path\n", path);
        return NULL;
    }
    char *p;
    int len = strlen(path);
    if (len == 1) {
        return fs_root;
    }
    char *str = strdup(path);
    for (p = str; *p; p++) {
        if (*p == '/') {
            *p = 0;
        }
    }
    p = str;
    while (len > 0) {
        if (*p) {
            if (node) {
                //kprintf("fs_namei - open %s, ref: %d, len: %d\n", 
                //node->name, node->ref_count, len);
                if (node->inode != 1) node->ref_count--;
            }
            if (!(node = fs_finddir(node ? node : fs_root, p))) {
                break;
            }
        }
        unsigned int curr_len = strlen(p);
        len = len - curr_len - 1;
        p = p + curr_len + 1;
    }
    free(str);
    return node;
}

#if 0
// needs more testing -> for now will only mount /dev/ files
int fs_mount(char *path, fs_node_t *node)
{

    KASSERT(*path);
    KASSERT(*path == '/');
    fs_node_t *n;

    n = fs_namei(path);
    if (!n) {
        // full path does not exits, check if parent node exists //
        // usually used in mount /dev/xxxx files
        char *dir, *base; // /di/r/base
        int len = strlen(path);
        dir = strdup(path);
        for (base = dir + len - 1; base >= dir; base--) {
            if (*base == '/') {
                *base = 0; base++; break;
            }
        }
        if(strlen(base) && !strcmp(node->name, base)) {
            n = fs_namei(dir);
            if(!n) {
                free(dir);
                return -1;
            }
            // add to parent //
            node->parent_inode = n->inode;
        }
        free(dir);
    }
    else {
        kprintf("MOUNT\n");
        // replace parent //
        node->parent_inode = n->parent_inode;
        node->ptr = n; // keep track of old node - n, so we can unmount later
        node->flags = FS_MOUNTPOINT;
        // node->parent_inode = n->inode;
    }
    kprintf("%d, %d, %s\n", node->inode, node->parent_inode, node->name);
    return 0;
}
#endif

fs_node_t *fs_creat(char *path, int mode) 
{
    char *dirname, *filename, *p;
    KASSERT(path[0]=='/');
    p = dirname = strdup(path);
    p += strlen(p);
    while (p > dirname && *(p - 1)!='/')
        p--;
    filename = strdup(p);
    *p = 0;
    fs_node_t *parent = fs_namei(dirname);
    if (!parent) {
        kprintf("cannot find parent %s\n", dirname);
        return NULL;
    }
    if (!parent->creat) {
        kprintf("file system does not support creat\n");
        return NULL;
    }
    return parent->creat(parent, filename, mode);
}

fs_node_t *fs_mknod(fs_node_t *dir, char *filename, uint32_t mode, uint32_t dev)
{
    return dir->mknod ? dir->mknod(dir, filename, mode, dev) : NULL;
}

int fs_truncate(fs_node_t *node, unsigned int length) 
{
    KASSERT(node);
    if (!node->truncate) {
        kprintf("filesystem does not support truncate\n");
        return -1;
    }
    return node->truncate(node, length);
}

// move part of this code to sysfile.c //
int fs_unlink(char *path)	
{
    char *p, filename[MAX_PATH_LEN], dirname[MAX_PATH_LEN];
    p = canonize_path(current_task->cwd, path);
    bname(p, dirname, filename);	
    fs_node_t *parent = fs_namei(dirname);
    fs_node_t *node = fs_finddir(parent, filename);
    if (!parent || parent->type != FS_DIRECTORY) {
        kprintf("no such directory: %s\n", dirname);
        return -1;
    }
    if(!node) {
        kprintf("filename does not exists %s\n", filename);
        return -1;
    }
    if(node->type == FS_DIRECTORY) {
        struct dirent *dir;
        bool found = false;
        int idx = 0;
        while ((dir = fs_readdir(node, idx++))) {
            if(dir->d_ino > 0) {
                if (strcmp(dir->d_name, ".") == 0) 
                    continue;
                if(strcmp(dir->d_name, "..") == 0)
                    continue;
                found = true;
                break;
            }
        }
        if (found) {
            kprintf("directory not empty\n");
            return -1;
        }
    }
    fs_close(node);
    if (!parent->unlink) {
        kprintf("%s does not support unlinking\n");
        return -1;
    }
    return parent->unlink(parent, filename);
}

int fs_link(fs_node_t *parent, fs_node_t *node, char *name) 
{
    if (!parent->link) {
        //serial_debug("node: %s does not support linking\n", parent->name);
    }
    return parent->link(parent, node, name);
}

int fs_ioctl(fs_node_t *node, int request, void *argp)
{
    return node->ioctl ? node->ioctl(node, request, argp) : -1;
}

// dep
int fs_rename(char *oldname, char *newname) 
{
    (void) oldname;
    (void) newname;
    return -1;
    /*
       int ret = 0;
       fs_node_t *oldp, *newp;
       fs_node_t *node;
       char oldb[MAX_PATH_LEN],	// old base name
       newb[MAX_PATH_LEN],	// new base name
       oldd[MAX_PATH_LEN],	// old dir name
       newd[MAX_PATH_LEN];	// new dir name
       bname(oldname, oldd, oldb);
       bname(newname, newd, newb);

       if(!(oldp = fs_namei(oldd))) {
    //serial_debug("fs_rename: cannot find old dir parent\n");
    return -1;
    }
    if(!(newp = fs_namei(newd))) {
    //serial_debug("fs_rename: cannot find new dir parent\n");
    return -1;
    }
    if(!(node = fs_finddir(oldp, oldb))) {
    //serial_debug("fs_rename: cannot find old node\n");
    return -1;
    }
    ret = node->rename(oldp, oldb, newp, newb);
    fs_close(node);
    fs_close(oldp);
    fs_close(newp);
    return ret;
    */
}

/**
 * TODO: rewrite this
 * check permissions
 * more...
 */
int fs_open_namei(char *path, int flags, int mode, fs_node_t **node)
{
    char *p;
    p = canonize_path(current_task->cwd, path);
    *node = fs_namei(p);
    if (!*node && flags & O_CREAT) {
        *node = fs_creat(p, mode);
    }
    if (node && (flags & O_TRUNC)) {
        if (flags & (O_RDWR | O_WRONLY)) {
            if (fs_truncate(*node, 0) < 0) {
                kprintf("cannot truncate %s\n", p);
                return -1;
            }
        }
    }
    return *node ? 0 : -1;
}

void lstree(fs_node_t *parent, int level)
{
    struct dirent *dir = 0;
    unsigned int i = 0;
    int j;
    if (!parent)
        parent = fs_root;
    kprintf("Listing directory: %s\n", parent->name);
    while ((dir = fs_readdir(parent, i++))) {
        if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
            continue;
        fs_node_t *file = fs_finddir(parent, dir->d_name);
        for (j = 0; j < level; j++)
            kprintf("    ");
        kprintf("%s - inode:%d, \n", file->name, file->inode);
        if (file->type & FS_DIRECTORY) {
            lstree(file, level+1);
        }
        fs_close(file);
    }
}

