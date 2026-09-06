#include "backup.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void backup_context_init(BackupContext *ctx) {
    if (ctx) {
        ctx->verbose = false;
        ctx->dry_run = false;
        ctx->files_linked = 0;
        ctx->space_saved = 0;
    }
}

static void log_verbose(BackupContext *ctx, const char *action, const char *src, const char *dst, const char *extra) {
    if (!ctx->verbose) return;
    if (extra) {
        printf("[%s] %s -> %s (target: %s)\n", action, src, dst, extra);
    } else if (dst) {
        printf("[%s] %s -> %s\n", action, src, dst);
    } else {
        printf("[%s] %s\n", action, src);
    }
}

static void create_hard_link(const char *src, const char *dst, off_t size, BackupContext *ctx) {
    log_verbose(ctx, "LINK", src, dst, NULL);
    
    if (!ctx->dry_run) {
        if (link(src, dst) == -1) {
            fprintf(stderr, "Error linking %s to %s: %s\n", src, dst, strerror(errno));
            return;
        }
    }
    
    ctx->files_linked++;
    ctx->space_saved += size;
}

static void copy_symlink(const char *src, const char *dst, BackupContext *ctx) {
    char target[PATH_MAX];
    ssize_t len = readlink(src, target, sizeof(target) - 1);
    
    if (len == -1) {
        fprintf(stderr, "Error reading symlink %s: %s\n", src, strerror(errno));
        return;
    }
    
    target[len] = '\0';
    log_verbose(ctx, "SYMLINK", src, dst, target);
    
    if (!ctx->dry_run) {
        if (symlink(target, dst) == -1) {
            fprintf(stderr, "Error creating symlink %s: %s\n", dst, strerror(errno));
        }
    }
}

static void copy_permissions(const char *src, const char *dst, BackupContext *ctx) {
    struct stat st;
    if (stat(src, &st) == -1) {
        fprintf(stderr, "Error stat (permissions) for %s: %s\n", src, strerror(errno));
        return;
    }
    
    if (!ctx->dry_run) {
        if (chmod(dst, st.st_mode) == -1) {
            fprintf(stderr, "Error chmod for %s: %s\n", dst, strerror(errno));
        }
    }
}

static void process_entry(const char *src_path, const char *dst_path, struct stat *st, BackupContext *ctx) {
    if (S_ISREG(st->st_mode)) {
        if (access(src_path, R_OK) == -1) {
            fprintf(stderr, "Warning: missing read permissions for file %s\n", src_path);
        }
        create_hard_link(src_path, dst_path, st->st_size, ctx);
    } else if (S_ISLNK(st->st_mode)) {
        copy_symlink(src_path, dst_path, ctx);
    } else if (S_ISDIR(st->st_mode)) {
        backup_directory(src_path, dst_path, ctx);
    }

    if (!S_ISLNK(st->st_mode)) {
        copy_permissions(src_path, dst_path, ctx);
    }
}

int backup_directory(const char *src, const char *dst, BackupContext *ctx) {
    if (access(src, R_OK | X_OK) == -1) {
        fprintf(stderr, "Error: missing read/execute permissions for directory %s: %s\n", src, strerror(errno));
        return -1;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        fprintf(stderr, "Error opening directory %s: %s\n", src, strerror(errno));
        return -1;
    }

    log_verbose(ctx, "MKDIR", dst, NULL, NULL);

    if (!ctx->dry_run) {
        if (mkdir(dst, 0755) == -1 && errno != EEXIST) {
            fprintf(stderr, "Error creating directory %s: %s\n", dst, strerror(errno));
            closedir(dir);
            return -1;
        }
    }

    char *src_path = malloc(PATH_MAX);
    char *dst_path = malloc(PATH_MAX);
    if (!src_path || !dst_path) {
        fprintf(stderr, "Error allocating memory\n");
        free(src_path);
        free(dst_path);
        closedir(dir);
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        int src_len = snprintf(src_path, PATH_MAX, "%s/%s", src, entry->d_name);
        int dst_len = snprintf(dst_path, PATH_MAX, "%s/%s", dst, entry->d_name);

        if (src_len >= PATH_MAX || dst_len >= PATH_MAX || src_len < 0 || dst_len < 0) {
            fprintf(stderr, "Error: path too long for %s\n", entry->d_name);
            continue;
        }

        struct stat st;
        if (lstat(src_path, &st) == -1) {
            fprintf(stderr, "Error lstat for %s: %s\n", src_path, strerror(errno));
            continue;
        }

        process_entry(src_path, dst_path, &st, ctx);
    }

    free(src_path);
    free(dst_path);
    closedir(dir);
    return 0;
}
