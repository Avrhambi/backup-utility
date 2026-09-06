# Backup Utility

A POSIX backup tool in C that snapshots a directory tree using **hard links for
regular files**, so unchanged data costs zero additional disk space while
permissions, symlinks, and the directory hierarchy are preserved exactly.

---

## 1. System Architecture & Flow

The program is split into an I/O-free CLI layer and a recursive traversal engine
that carries all mutable state in a single `BackupContext` struct (no globals).

```
argv ──▶ parse_arguments (getopt_long)      main.c
             │  populates BackupContext {verbose, dry_run, counters}
             ▼
        validate_directories                main.c
             │  src is a dir + R_OK|X_OK; dst must NOT already exist
             ▼
        backup_directory(src, dst) ◀───────┐  backup.c  (recursive)
             │  opendir + mkdir(dst,0755)  │
             ▼                             │
        for each dirent (skip . ..):       │
          lstat(entry)                     │
             ├─ S_ISREG  ─▶ link()  ───────┼─▶ files_linked++, space_saved += size
             ├─ S_ISLNK  ─▶ readlink + symlink()
             ├─ S_ISDIR  ─▶ backup_directory(...)  ── recurse ──┘
             └─ other     ─▶ ignored
          chmod(dst) to mirror source mode (except symlinks)
             ▼
        write_summary                       main.c
             dst/backup.log  (or stdout on --dry-run)
```

### End-to-end walkthrough

1. **CLI parse** — `./backup -v /src /dst`. `getopt_long` sets `verbose` /
   `dry_run` on the context; exactly two positional args are required.
2. **Validation** — `/src` must exist, be a directory, and be readable +
   searchable (`R_OK | X_OK`). `/dst` must **not** already exist (outside
   dry-run), preventing accidental merges into a populated tree.
3. **Directory creation** — `mkdir(dst, 0755)`, tolerating `EEXIST`.
4. **Traversal** — `opendir`/`readdir` walk `/src`; each entry is classified with
   `lstat` (not `stat`, so symlinks are seen as symlinks).
5. **Replication** — regular files become hard links (`link`), symlinks are
   recreated from their `readlink` target, subdirectories recurse.
6. **Metadata** — `chmod` copies the source mode onto every non-symlink entry.
7. **Summary** — `files_linked` and `space_saved` are written to
   `dst/backup.log`, or printed to stdout under `--dry-run`.

---

## 2. Tech Stack & Engineering Decisions

| Layer | Choice | Rationale & trade-offs |
| :--- | :--- | :--- |
| Language | C (C99, `-Wall -Wextra`) | Direct access to `link`, `lstat`, `readlink`. Cost: manual memory management and explicit `snprintf` truncation checks on every path join. |
| Structure | `main.c` (CLI) + `backup.c/.h` (engine) | CLI parsing and traversal are separately testable; the engine has no `printf` policy baked in beyond an opt-in verbose logger. |
| State | `BackupContext` struct threaded through calls | No global mutable state; recursion stays reentrant and the counters have one owner. |
| Copy strategy | Hard links for regular files | 0 bytes for unchanged files, instant vs. byte copy. Trade-offs: cannot cross filesystems/partitions (`EXDEV`), and editing a backed-up file mutates the original inode — this is a snapshot of *structure*, not an isolated copy. |
| Symlinks | `readlink` + `symlink` (never dereferenced) | Broken or relative links are reproduced verbatim rather than followed or flattened. |
| Arg parsing | `getopt_long` | Short and GNU long options for free; GNU-specific, matching the POSIX target. |

**Not applicable to this project:** database/indexing section — there is no
datastore; the only persisted artifact is a plaintext `backup.log`.

---

## 3. Resilience & Error Handling Patterns

- **Non-existent destination guard** — `validate_directories` refuses a `/dst`
  that already exists, so a backup can never silently interleave with unrelated
  files. (`main.c:68`)
- **Fail-soft per entry** — a failed `link`, `readlink`, `symlink`, `chmod`, or
  `lstat` prints to `stderr` and continues to the next entry; one unreadable file
  does not abort the run. (`backup.c:40`, `backup.c:54`, `backup.c:145`)
- **Fail-hard per directory** — an unreadable directory or failed `mkdir` returns
  `-1` and unwinds, because nothing useful can be written beneath it.
  (`backup.c:101`, `backup.c:115`)
- **Path-length safety** — every `src/name` join checks the `snprintf` return
  against `PATH_MAX` and skips the entry on truncation rather than acting on a
  clipped path. (`backup.c:139`)
- **`lstat` over `stat`** — symlinks are classified without being followed,
  avoiding infinite recursion through self-referential links.
- **Dry-run** — `--dry-run` walks and counts through the identical code path with
  every mutating syscall gated behind `ctx->dry_run`, so the preview matches the
  real run.
- **Allocation checks** — the per-directory path buffers are heap-allocated and
  null-checked, freeing the partner buffer and closing the `DIR*` on failure.

---

## 4. Project Layout

```
Backup_Utility/
├── main.c        CLI: getopt_long parsing, directory validation, log summary
├── backup.c      Engine: recursive traversal, hard-link / symlink / chmod replication
├── backup.h      BackupContext struct + public engine API
├── Makefile      gcc -Wall -Wextra -g; objects -> backup
└── .gitignore    build artifacts (backup, *.o)
```

Flat by design — two translation units, one shared header. The boundary that
matters (CLI vs. filesystem engine) is the file split.

---

## 5. Local Setup & Quickstart

```bash
git clone https://github.com/Avrhambi/Backup_Utility.git
cd Backup_Utility
make                      # produces ./backup

# smoke test on a throwaway tree
mkdir -p /tmp/src/sub && echo hi > /tmp/src/a.txt && ln -s a.txt /tmp/src/link
./backup --dry-run -v /tmp/src /tmp/dst   # preview, writes nothing
./backup -v /tmp/src /tmp/dst             # real run
cat /tmp/dst/backup.log
ls -li /tmp/src/a.txt /tmp/dst/a.txt      # identical inode number = hard link

make clean
```

### Usage

```
./backup [OPTIONS] <source_dir> <backup_dir>
  -h, --help      print usage
  -v, --verbose   print each LINK / SYMLINK / MKDIR action
  -d, --dry-run   simulate; count files and bytes, write nothing
```

---

## 6. Testing & CI/CD

> **Gap — no automated tests or CI yet.** The build is `make` with
> `-Wall -Wextra` (no warnings) and manual dry-run verification against the tree
> above. There is no unit-test target and no `.github/workflows/`.
>
> Planned: a shell-based fixture test (build a known tree, run the tool, assert
> inode equality for files, target equality for symlinks, mode equality via
> `stat -c %a`, and the `backup.log` counts) wired into a GitHub Actions job that
> runs `make` + the fixture script on every push.

---

## 7. Performance

> **Gap — no benchmark evidence.** The hard-link strategy is O(number of entries)
> syscalls with no byte copying, so it is expected to dominate `cp -r` on large
> unchanged trees, but this has not been measured against `cp` or `rsync`.
>
> To produce real figures: generate a fixed corpus (e.g. `N` files totalling
> `X` GiB), then `time ./backup ...` vs. `time cp -r ...` vs. `time rsync -a ...`
> on the same corpus and record wall time + `du -sh` of each destination.
