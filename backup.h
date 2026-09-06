#ifndef BACKUP_H
#define BACKUP_H

#include <sys/types.h>
#include <stdbool.h>

typedef struct {
    bool verbose;
    bool dry_run;
    unsigned long files_linked;
    unsigned long long space_saved;
} BackupContext;

void backup_context_init(BackupContext *ctx);
int backup_directory(const char *src, const char *dst, BackupContext *ctx);

#endif // BACKUP_H
