#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

int main(int argc, char *argv[])
{
    DIR *d;
    struct dirent *dir;
    char *path, buf[512];
    struct stat st;

    if (argc < 2) {
        path = strdup(".");
    } else {
        path = strdup(argv[1]);
    }

    if (stat(path, &st) < 0) {
        printf("ls: cannot stat %s\n", path);
        return 1;
    }
    if (S_ISFIFO(st.st_mode)) {
        printf("%s, pipe, mode: %o, inode: %d, size: %d\n",
                path, st.st_mode & ~S_IFMT, st.st_ino, st.st_size);
    } else if(S_ISCHR(st.st_mode)) {
        printf("%s, char device, mode:%o, inode:%d, major:%d, minor:%d\n",
                path, st.st_mode & ~S_IFMT, st.st_ino,
                major(st.st_rdev), minor(st.st_rdev));
    } else if(S_ISBLK(st.st_mode)) {
        printf("%s, block device, mode:%o, inode:%d, major:%d, minor:%d\n",
                path, st.st_mode & ~S_IFMT, st.st_ino,
                major(st.st_rdev), minor(st.st_rdev));
    } else if (S_ISREG(st.st_mode)) {
        printf("%s, regular, mode:%o, inode:%d size:%d\n",
                path, st.st_mode & ~S_IFMT, st.st_ino, st.st_size);
    } else if(S_ISDIR(st.st_mode)) {
        printf("type ino  size   name\n");
        if(!(d = opendir(path))) {
            printf("cannot open %s\n", path);
        }
        while ((dir = readdir(d))) {
            if (dir->d_ino == 0)
                continue;
            strncpy(buf, path, sizeof(buf)-1);
            strcat(buf, "/");
            strncat(buf, dir->d_name, sizeof(buf) - 1 - strlen(buf) - 1);
            if (stat(buf, &st) < 0) {
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            if (S_ISFIFO(st.st_mode))
                printf("f");
            else if (S_ISCHR(st.st_mode))
                printf("c");
            else if (S_ISDIR(st.st_mode))
                printf("d");
            else if (S_ISBLK(st.st_mode))
                printf("b");
            else if(S_ISREG(st.st_mode))
                printf("-");
            else if (S_ISLNK(st.st_mode))
                printf("l");
            else if (S_ISSOCK(st.st_mode))
                printf("s");
            printf("    %-4d %-6d %s\n", st.st_ino, st.st_size, dir->d_name);
        }
        closedir(d);
        return 0;
    }
}
