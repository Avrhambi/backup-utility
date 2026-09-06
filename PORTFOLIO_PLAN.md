# Portfolio Transformation Plan: POSIX Backup Utility

## 🎯 The Vision
Transform the university exercise (`OS_EX2/p3/backup.c`) lcaotion:C:\Users\avrha\Documents\my_technical_knowledge\CS_Courses\שנה ג\מערכות הפעלה\תרגילים into a production-ready command-line tool named **"POSIX-Compliant File System Backup Utility"**. This proves proficiency with low-level Linux file system APIs, storage optimization (hard links), and edge-case handling.

---

## 🚀 Phase 1: Extraction & Standalone Setup
*Goal: Break the code out of the "homework" structure.*
1. **Isolate the Code:** Copy `backup.c` from `OS_EX2/p3` into this folder.
2. **Rename & Clean:** Rename the source file to something professional like `backup_util.c`. Remove university comments, student IDs, or references to "Exercise 2".
3. **Build System:** Create a standard `Makefile` that compiles the code into an executable named `backup`.

## 🛠️ Phase 2: Reliability & Features
*Goal: Ensure the tool handles real-world usage without crashing.*
1. **Add CLI Flags (`getopt`):**
   * `-h` / `--help`: Prints usage instructions.
   * `-v` / `--verbose`: Prints exactly what the tool is doing in real-time (e.g., `[LINK] /src/file -> /dest/file`).
   * `-d` / `--dry-run`: (Optional but impressive) Simulates the backup without actually writing files, showing what *would* happen.
2. **Robust Error Handling:**
   * Ensure the program checks if the source directory actually exists before running.
   * Ensure it checks for proper Read/Write permissions.
   * Replace silent crashes (segfaults) with graceful exits using `perror()` to tell the user exactly what failed.
3. **Logging (Optional):**
   * Output a `backup.log` file in the destination folder summarizing the backup time, number of files linked, and total space saved.

## 📚 Phase 3: Documentation
*Goal: Pass the recruiter screen.*
1. **Write `README.md`:**
   * Include the Elevator Pitch (focusing on zero-storage hard links).
   * Provide a clear "How to use" section with examples.
   * Explain the "Why": Describe the problem it solves (storage bloat, broken symlinks, lost permissions).
2. **Clean Code:** Run Valgrind to ensure 0 memory leaks when dynamically allocating memory for file paths during directory traversal.
