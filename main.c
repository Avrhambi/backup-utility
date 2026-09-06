#include "backup.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <time.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <source_dir> <backup_dir>\n", prog_name);
    printf("Options:\n");
    printf("  -h, --help      Print this help message\n");
    printf("  -v, --verbose   Print exactly what the tool is doing\n");
    printf("  -d, --dry-run   Simulate the backup without writing files\n");
}

static int parse_arguments(int argc, char *argv[], BackupContext *ctx) {
    int opt;
    static struct option long_options[] = {
        {"help",    no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"dry-run", no_argument, 0, 'd'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvd", long_options, NULL)) != -1) {
        switch (opt) {
            case 'v': ctx->verbose = true; break;
            case 'd': ctx->dry_run = true; break;
            case 'h': 
                print_help(argv[0]); 
                return 1;
            default: 
                print_help(argv[0]); 
                return -1;
        }
    }

    if (optind + 2 != argc) {
        fprintf(stderr, "Usage: %s [OPTIONS] <source_dir> <backup_dir>\n", argv[0]);
        return -1;
    }

    return 0;
}

static int validate_directories(const char *src_dir, const char *dst_dir, BackupContext *ctx) {
    struct stat src_stat, dst_stat;

    if (stat(src_dir, &src_stat) == -1 || !S_ISDIR(src_stat.st_mode)) {
        fprintf(stderr, "Error accessing source directory %s: %s\n", src_dir, strerror(errno));
        return -1;
    }

    if (access(src_dir, R_OK | X_OK) == -1) {
        fprintf(stderr, "Error: source directory %s lacks read/execute permissions\n", src_dir);
        return -1;
    }

    if (!ctx->dry_run && stat(dst_dir, &dst_stat) != -1) {
        fprintf(stderr, "Error: backup directory %s already exists\n", dst_dir);
        return -1;
    }

    return 0;
}

static void write_summary(const char *dst_dir, BackupContext *ctx) {
    if (!ctx->dry_run) {
        char log_path[PATH_MAX];
        snprintf(log_path, sizeof(log_path), "%s/backup.log", dst_dir);
        
        FILE *log_file = fopen(log_path, "w");
        if (log_file) {
            time_t now = time(NULL);
            fprintf(log_file, "Backup Time: %s", ctime(&now));
            fprintf(log_file, "Files Linked: %lu\n", ctx->files_linked);
            fprintf(log_file, "Total Space Saved: %llu bytes\n", ctx->space_saved);
            fclose(log_file);
        } else {
            fprintf(stderr, "Error creating log file %s: %s\n", log_path, strerror(errno));
        }
    } else {
        printf("\n[DRY RUN SUMMARY]\n");
        printf("Files Linked: %lu\n", ctx->files_linked);
        printf("Total Space Saved: %llu bytes\n", ctx->space_saved);
    }
}

int main(int argc, char *argv[]) {
    BackupContext ctx;
    backup_context_init(&ctx);

    int parse_res = parse_arguments(argc, argv, &ctx);
    if (parse_res != 0) {
        return (parse_res > 0) ? 0 : 1;
    }

    const char *src_dir = argv[optind];
    const char *dst_dir = argv[optind + 1];

    if (validate_directories(src_dir, dst_dir, &ctx) != 0) {
        return 1;
    }

    if (backup_directory(src_dir, dst_dir, &ctx) != 0) {
        return 1;
    }

    write_summary(dst_dir, &ctx);

    return 0;
}
