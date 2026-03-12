# C Backup & Recovery Utility

![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)

This repository provides a C-based command-line utility to securely backup, track, remove, and recover files and directories. It leverages MD5 hashing to ensure data integrity, prevents redundant backups of unchanged files, and provides an interactive CLI for managing backup versions.

---

## Repository Structure
```
.  
├── Makefile  
├── main.c  
├── backup.h  
├── backup.c  
├── recover.c  
├── remove.c  
├── list.c  
├── help.c  
└── utils.c  
```
- **backup.c** Handles the core logic for copying files to the backup directory. Supports directory recursion (`-r`), directory-only scoping (`-d`), and forced overwriting (`-y`). Uses MD5 hashing to prevent duplicate backups of the same file.
- **recover.c** Restores files from the backup directory to their original (or newly specified) paths. Handles multiple backup versions by prompting the user to select the desired timestamp.
- **remove.c** Deletes backed-up files and cleans up empty directories in the backup tree. Supports forced deletion of all versions (`-a`).
- **list.c** Provides an interactive, visual tree-structure representation of backed-up files. From this interactive prompt, users can directly remove (`rm`), recover (`rc`), or inspect (`vi`/`vim`) files.
- **utils.c & main.c** Contains system initialization, MD5 hash calculation, timestamp generation, linked-list logging (`backup.log`), and the main command parser.

---

## Prerequisites
> **IMPORTANT** : This tool relies on the Linux `proc` filesystem (e.g., `/proc/self/exe`) and specific POSIX system calls. Execution on non-Linux environments (like Windows natively) is **NOT** recommended.

1. **Linux OS** (Kernel support for standard POSIX file operations).
2. **GCC** (version >= 7.0).
3. **OpenSSL Library** (required for MD5 hash calculations).
4. **Vim** (optional, but required if you use the `vi`/`vim` command inside the interactive `list` prompt).

---

## Unpacking Archives  
If this repository is distributed as a compressed archive (e.g., `backup.tar.gz`), use tar to unpack it.
```bash
    tar -xzvf backup.tar.gz
    cd backup
```
**Note**: Must follow the extracted directory structure to ensure the `Makefile` resolves dependencies correctly.

---

## Installation & Setup

1. **Install dependencies** (OpenSSL and build tools):
   ```bash
   sudo apt update
   sudo apt install -y build-essential libssl-dev
2. **Clone or copy** this repository to your local machine.
3. **Compile the program**:
   ```bash
    make
    ```
    This will gererate the executable binary named **backup**.
4. **Clean build files** (optional):
    ```bash
    make clean

**Note**: By default, the program sets the backup directory to `/home/backup` (defined in `utils.c`). You must ensure you have write permission to this path, or modify `backupdir` in `utils.c` before compiling. You can get permission to `/home/backup` via giving `sudo` permission.

---

## Commands & Usage
The program provides five main commands: `backup`, `remove`, `recover`, `list`, and `help`.

1. **Backup(`backup`)**  
Copies a file or directory to the backup storage. It creates a timestamped folder (e.g., `YYMMDDHHMMSS`) to keep track of versions.
    ```bash
    ./backup backup <PATH> [OPTIONS]
    ```
- `-d`: Backup files inside a directory (shallow).
  
- `-r`: Backup files in a directory recursively.
  
- `-y`: Force backup even if an identical file (same MD5 hash) is already backed up.
2. **Remove(`remove`)**  
Deletes backed-up files from the repository. If multiple versions exist, it will prompt you to choose one unless `-a` is used.
    ```bash
    ./backup remove <PATH> [OPTIONS]
    ```
- `-d`: Remove backed-up files in a directory (shallow).

- `-r`: Remove backed-up files in a directory recursively.

- `-a`: Remove **all** backup versions of the specified file without prompting.
3. **Recover(`recover`)**
Restores a backed-up file to its origin location or a new specified path.
    ```bash
    ./backup recover <PATH> [OPTIONS]
    ```
- `-d`: Recover backed-up files in a directory (shallow).
  
- `-r`: Recover backed-up files in a directory recursively.
  
- `-n <NEW_PATH>`: Recover the file(s) to `<NEW_PATH>` instead of the original origin.
4. **Interactive List (`list`)**  
Displays a visual tree of backed-up files for a given directory and opens an interactive prompt.
    ```bash
    ./backup list [PATH]
    ```
    Inside the interactive prompt (`>>`), you can use:  

   - `rm <INDEX> [OPTION]`: Remove the backed-up file at `<INDEX>`.

   - `rc <INDEX> [OPTION]`: Recover the backed-up file at `<INDEX>`.

   - `vi <INDEX>` or `vim <INDEX>`: Open the original file in the Vim editor.

   - `exit`: Quit the interactive prompt.
  
---

## Diretory Layout Example
```
/home/backup/
├── backup.log     # Centralized log of all operations
├── 250606123000/  # Timestamped backup folder (YYMMDDHHMMSS)
│   └── example.c
└── 250606123500/
    └── subdir/
        └── file.txt
```
The `backup.log` is automatically updated and tracks original paths and backup paths.

---

## Example Workflow
1. **Backup a single file**:
   ```bash
   ./backup backup ./src/example.c
    "example.c" backuped to "/home/backup/YOURDATETIME/example.c"
2. **backup a directory recursively**:
   ```bash
   ./backup backup ./src -r
   ... [Multiple files will be backed-up] ...
3. **List backups and enter the interactive prompt**:
   ```bash
    ./backup list
    ... [Visual tree of backed-up files with index numbers] ...
    >>
4. **Recover a file using its index from the interactive prompt**:
    ```bash
    >> rc 1
    "/home/backup/YOURDATETIME/example.c" recovered to "./src/example.c"
5. **Exit the interactive prompt**:
   ```bash
   >> exit
---

## Troubleshooting
- **"openssl/md5.h: No such file or directory"**:  You are missing the OpenSSL development headers. Run this.
  ```bash
  sudo apt install libssl-dev.
  ```
- **"open error for /home/backup/backup.log"** or **Permission Denied** : The default backup directory is hardcoded to `/home/backup`. If your user lacks permissions to create or write to `/home`, run the program with `sudo`, or change the `backupdir` variable in `utils.c` to a path you own (e.g., `"/home/yourusername/backup"`).

 - **"invalid command" or segmentation faults on commands** : Ensure you provide the absolute or correct relative `<PATH>` as the immediate next argument after the command, followed by the options (e.g., `./backup backup ./myfolder -r`).

- **Interactive `list` prompt keeps repeating or fails** : The interactive prompt reads via `stdin`. Ensure you are typing the exact index number listed on the left side of the tree output (e.g., `rm 2 -a`).
  
---

## Customization
- **Change Default Backup Directory**: In `utils.c`, locate the global variable:
  ```C
  char *backupdir = "/home/backup";
  ```
    Change this to your preferred path (e.g., a hidden folder in your home directory) before running `make`.
- **Hash Algorithm**: Hash Algorithm The project currently uses MD5 via `<openssl/md5.h>`. If higher security is needed, you can replace the logic in `calc_md5_hash()` inside `utils.c` to use SHA-256 (`<openssl/sha.h>`).

---

## License & Acknowledgments

This source code is licensed under the **MIT License**. See the `LICENSE` file for details.  

**Acknowledgments:**
- **Code Implementation**: Wooyong Eom, CSE, Soongsil Univ.
- **Project Specification**: The architecture, requirements, and specifications for this project were provided as an assignment for the Linux System Programming course at Soongsil University, by Prof. Hong Jiman, OSLAB. This repository contains my original implementation of those requirements.
---
_Last Updated: March 11, 2026_
