# Backup Utility

A POSIX-compliant, high-performance backup tool written in C.

## Elevator Pitch

**Zero-Storage Backups via Hard Links**
Why duplicate data when you don't have to? This backup utility intelligently creates backups by utilizing hard links for regular files instead of duplicating their content. This means your backups take up *zero* extra disk space for unmodified files, while fully preserving file integrity, metadata, and the directory hierarchy. 

## The "Why"

This tool was built to solve several common problems with naive backups:
1. **Storage Bloat:** Standard `cp -r` backups quickly consume all available disk space by copying the identical file contents multiple times. Hard links completely eliminate this waste.
2. **Broken Symlinks:** Instead of blindly copying what a symlink points to (or breaking the link), this tool replicates the exact symlink itself, ensuring that references remain intact.
3. **Lost Permissions:** File permissions and modes are automatically copied over, ensuring that executable files remain executable and private files remain private in the backup.

## How to Use

Compile the program using the provided `Makefile`:

```bash
make
```

### Usage

```bash
./backup [OPTIONS] <source_dir> <backup_dir>
```

### Options
* `-h, --help`: Prints usage instructions.
* `-v, --verbose`: Prints exactly what the tool is doing in real-time (e.g., `[LINK] /src/file -> /dest/file`).
* `-d, --dry-run`: Simulates the backup without actually writing files or directories. Useful for seeing what *would* happen.

### Examples

**Standard Backup:**
```bash
./backup /home/user/docs /mnt/backups/docs_backup
```

**Dry-Run (Test before committing):**
```bash
./backup --dry-run /home/user/docs /mnt/backups/docs_backup
```
*Observe exactly how many files will be backed up and how much space will be saved, without writing anything to disk.*

**Verbose Mode:**
```bash
./backup -v /home/user/docs /mnt/backups/docs_backup
```
*See every file, directory, and symlink as it is processed in real-time.*
