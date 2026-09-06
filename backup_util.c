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
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int opt_verbose = 0;
int opt_dry_run = 0;

unsigned long log_files_linked = 0;
unsigned long long log_space_saved = 0;

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <source_dir> <backup_dir>\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help      Print this help message\n");
    printf("  -v, --verbose   Print exactly what the tool is doing\n");
    printf("  -d, --dry-run   Simulate the backup without writing files\n");
}

void create_hard_link(const char *src, const char *dst, off_t size) {
    if (opt_verbose) {
        printf("[LINK] %s -> %s\n", src, dst);
    }
    if (!opt_dry_run) {
        if (link(src, dst) == -1) {
            fprintf(stderr, "Error linking %s to %s: %s\n", src, dst, strerror(errno));
            return;
        }
    }
    log_files_linked++;
    log_space_saved += size;
}

void copy_symlink(const char *src, const char *dst) {
    char target[PATH_MAX];
    ssize_t len = readlink(src, target, sizeof(target) - 1);
    if (len == -1) {
        fprintf(stderr, "Error reading symlink %s: %s\n", src, strerror(errno));
        return;
    }
    target[len] = '\0';
    if (opt_verbose) {
        printf("[SYMLINK] %s -> %s (target: %s)\n", src, dst, target);
    }
    if (!opt_dry_run) {
        if (symlink(target, dst) == -1) {
            fprintf(stderr, "Error creating symlink %s: %s\n", dst, strerror(errno));
        }
    }
}

void copy_permissions(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) == -1) {
        fprintf(stderr, "Error stat (permissions) for %s: %s\n", src, strerror(errno));
        return;
    }
    if (!opt_dry_run) {
        if (chmod(dst, st.st_mode) == -1) {
            fprintf(stderr, "Error chmod for %s: %s\n", dst, strerror(errno));
        }
    }
}

void copy_directory(const char *src, const char *dst) {
    if (access(src, R_OK | X_OK) == -1) {
        fprintf(stderr, "Error: missing read/execute permissions for directory %s: %s\n", src, strerror(errno));
        return;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", src, strerror(errno));
        return;
    }

    if (opt_verbose) {
        printf("[MKDIR] %s\n", dst);
    }
    if (!opt_dry_run) {
        if (mkdir(dst, 0755) == -1 && errno != EEXIST) {
            fprintf(stderr, "Error creating directory %s: %s\n", dst, strerror(errno));
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
            fprintf(stderr, "Error lstat for %s: %s\n", src_path, strerror(errno));
            continue;
        }

        if (S_ISREG(st.st_mode)) {
            if (access(src_path, R_OK) == -1) {
                fprintf(stderr, "Warning: missing read permissions for file %s\n", src_path);
            }
            create_hard_link(src_path, dst_path, st.st_size);
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
        fprintf(stderr, "Error accessing source directory %s: %s\n", src_dir, strerror(errno));
        return 1;
    }

    if (access(src_dir, R_OK | X_OK) == -1) {
        fprintf(stderr, "Error: source directory %s lacks read/execute permissions\n", src_dir);
        return 1;
    }

    if (!opt_dry_run && stat(dst_dir, &dst_stat) != -1) {
        fprintf(stderr, "Error: backup directory %s already exists\n", dst_dir);
        return 1;
    }

    copy_directory(src_dir, dst_dir);

    if (!opt_dry_run) {
        char log_path[PATH_MAX];
        snprintf(log_path, sizeof(log_path), "%s/backup.log", dst_dir);
        FILE *log_file = fopen(log_path, "w");
        if (log_file) {
            time_t now = time(NULL);
            fprintf(log_file, "Backup Time: %s", ctime(&now));
            fprintf(log_file, "Files Linked: %lu\n", log_files_linked);
            fprintf(log_file, "Total Space Saved: %llu bytes\n", log_space_saved);
            fclose(log_file);
        } else {
            fprintf(stderr, "Error creating log file %s: %s\n", log_path, strerror(errno));
        }
    } else {
        printf("\n[DRY RUN SUMMARY]\n");
        printf("Files Linked: %lu\n", log_files_linked);
        printf("Total Space Saved: %llu bytes\n", log_space_saved);
    }

    return 0;
}
