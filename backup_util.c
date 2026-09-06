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
#include <getopt.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int opt_verbose = 0;
int opt_dry_run = 0;

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <source_dir> <backup_dir>\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help      Print this help message\n");
    printf("  -v, --verbose   Print exactly what the tool is doing\n");
    printf("  -d, --dry-run   Simulate the backup without writing files\n");
}

void create_hard_link(const char *src, const char *dst) {
    if (opt_verbose) {
        printf("[LINK] %s -> %s\n", src, dst);
    }
    if (!opt_dry_run) {
        if (link(src, dst) == -1) {
            perror("link");
        }
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
    if (opt_verbose) {
        printf("[SYMLINK] %s -> %s (target: %s)\n", src, dst, target);
    }
    if (!opt_dry_run) {
        if (symlink(target, dst) == -1) {
            perror("symlink");
        }
    }
}

void copy_permissions(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) == -1) {
        perror("stat (permissions)");
        return;
    }
    if (!opt_dry_run) {
        if (chmod(dst, st.st_mode) == -1) {
            perror("chmod");
        }
    }
}

void copy_directory(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("opendir");
        return;
    }

    if (opt_verbose) {
        printf("[MKDIR] %s\n", dst);
    }
    if (!opt_dry_run) {
        if (mkdir(dst, 0755) == -1) {
            perror("mkdir");
            closedir(dir);
            return;
        }
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
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

        if (!S_ISLNK(st.st_mode)) {
            copy_permissions(src_path, dst_path);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    int opt;
    static struct option long_options[] = {
        {"help",    no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"dry-run", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvd", long_options, NULL)) != -1) {
        switch (opt) {
            case 'v': opt_verbose = 1; break;
            case 'd': opt_dry_run = 1; break;
            case 'h': print_help(argv[0]); return 0;
            default: print_help(argv[0]); return 1;
        }
    }

    if (optind + 2 != argc) {
        fprintf(stderr, "Usage: %s [OPTIONS] <source_dir> <backup_dir>\n", argv[0]);
        return 1;
    }

    const char *src_dir = argv[optind];
    const char *dst_dir = argv[optind + 1];

    struct stat src_stat, dst_stat;

    if (stat(src_dir, &src_stat) == -1 || !S_ISDIR(src_stat.st_mode)) {
        perror("src dir");
        return 1;
    }

    if (!opt_dry_run && stat(dst_dir, &dst_stat) != -1) {
        perror("backup dir");
        return 1;
    }

    copy_directory(src_dir, dst_dir);

    return 0;
}
