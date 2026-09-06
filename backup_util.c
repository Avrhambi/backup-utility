#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>  

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


void create_hard_link(const char *src, const char *dst) {
    if (link(src, dst) == -1) {
        perror("link");
    }
}

void copy_symlink(const char *src, const char *dst) {
    char target[PATH_MAX];
    ssize_t len = readlink(src, target, sizeof(target) - 1);
    if (len == -1) {
        perror("readlink");
        return;
    }
    target[len] = '\0';
    if (symlink(target, dst) == -1) {
        perror("symlink");
    }
}

void copy_permissions(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) == -1) {
        perror("stat (permissions)");
        return;
    }
    if (chmod(dst, st.st_mode) == -1) {
        perror("chmod");
    }
}

void copy_directory(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("opendir");
        return;
    }

    // Create destination directory
    if (mkdir(dst, 0755) == -1) {
        perror("mkdir");
        closedir(dir);
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char src_path[PATH_MAX], dst_path[PATH_MAX];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        struct stat st;

        if (lstat(src_path, &st) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            create_hard_link(src_path, dst_path);
        } else if (S_ISLNK(st.st_mode)) {
            copy_symlink(src_path, dst_path);
        } else if (S_ISDIR(st.st_mode)) {
            copy_directory(src_path, dst_path);
        }

        // Preserve permissions for directories and files (not for symlinks)
        if (!S_ISLNK(st.st_mode)) {
            copy_permissions(src_path, dst_path);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source_dir> <backup_dir>\n", argv[0]);
        return 1;
    }

    struct stat 
    src_stat, 
    dst_stat;

    // Check source dir
    if (stat(argv[1], &src_stat) == -1 || !S_ISDIR(src_stat.st_mode)) {
        perror("src dir");
        return 1;
    }

    // Check that backup dir does not already exist
    if (stat(argv[2], &dst_stat) != -1) {
        perror("backup dir");
        return 1;
    }

    copy_directory(argv[1], argv[2]);

    return 0;
}
