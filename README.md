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

## System Architecture & Flow

The core logic relies on a recursive directory traversal loop, checking file types via `lstat`, and applying the appropriate POSIX syscalls to replicate the structure without duplicating data.

```mermaid
flowchart TD
    Start([Start Backup]) --> ReadDir[Read Directory Entry]
    ReadDir --> Lstat{File Type?}
    
    Lstat -- Directory --> Mkdir[Create Dest Dir & Copy Perms]
    Mkdir --> Recurse[Recursive Call]
    Recurse --> ReadDir
    
    Lstat -- Regular File --> Link[Create Hard Link (link)]
    Link --> ReadDir
    
    Lstat -- Symlink --> Readlink[Read Symlink Target]
    Readlink --> Symlink[Create New Symlink (symlink)]
    Symlink --> ReadDir
    
    Lstat -- Other --> Skip[Skip / Log Ignore]
    Skip --> ReadDir
    
    ReadDir -- EOF --> End([End Backup])
```

### End-to-End Walkthrough

1. **CLI Parsing**: The user invokes `./backup -v /src /dst`. `getopt` parses flags.
2. **Validation**: The tool checks if `/src` exists and if the user has read/execute permissions.
3. **Directory Creation**: `/dst` is created (if missing) and permissions are cloned from `/src`.
4. **Traversal**: `opendir` reads the contents of `/src` recursively.
5. **Hard Linking**: For every regular file found, `link(src_file, dst_file)` is called, creating a zero-storage pointer to the original inode.
6. **Logging**: If `-v` is active, the operation is printed. Upon completion, a summary is appended to `backup.log`.

## Tech Stack & Engineering Decisions

| Layer | Technology | Rationale & Trade-offs |
| :--- | :--- | :--- |
| **Language** | C (C99) | Chosen for direct, low-level access to POSIX system calls (`lstat`, `link`). Trade-off: Requires manual memory management and bounds checking (e.g., preventing `snprintf` overflows). |
| **File I/O** | POSIX Syscalls | `opendir`/`readdir`/`link` provide raw performance over standard library wrappers. Trade-off: Code is not portable to non-POSIX systems like native Windows. |
| **Storage Strategy**| Hard Links | Saves 100% of disk space for unmodified files. Trade-off: Hard links cannot span across different disk partitions or filesystems; modifying the backup file modifies the source. |

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

## 3. Performance Benchmarks

While this tool is designed to be a high-performance utility that utilizes hard links to save time and I/O overhead, we currently lack concrete benchmarking evidence against other tools (like `rsync` or `cp`). 

<!-- TODO: run EXPLAIN ANALYZE, paste real output -->
> **Gap Identified**: Performance claims currently lack empirical benchmark evidence.

## 4. Reliability & CI

The code contains robust error handling, but we do not yet have automated pipelines.

<!-- TODO: set up CI pipeline (e.g., GitHub Actions) and insert build badges here -->
> **Gap Identified**: Claims of reliability lack automated Continuous Integration (CI) and unit test evidence.
