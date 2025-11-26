# Project 2: Smash Shell
**Author**: Yifan Wan

## About This Project
**Smash** (Super Minimal Awesome Shell) is a feature-rich command-line interface designed for the xv6 operating system (RISC-V). It serves as the primary interface between the user and the kernel, replacing the default `sh`.

Smash is designed to mimic the behavior of modern Unix shells like `bash` or `zsh`, supporting advanced features such as process pipelines, I/O redirection, background job execution, and command history management. Additionally, significant modifications were made to the xv6 kernel to support features like script execution (Shebang) and file appending.

## Features

### 1. Interactive Prompt
The shell displays a dynamic prompt containing useful context:
`[Status]-[Count]─[Directory]$`
*   **Status**: The exit code of the previous command (0 for success, non-zero for failure).
*   **Count**: The sequential number of the current command.
*   **Directory**: The current working directory (e.g., `/home` or `/`).

### 2. Built-in Commands
Smash handles the following commands internally (without forking):
*   `cd <path>`: Changes the current working directory.
*   `exit`: Terminates the shell session.
*   `history [-t]`: Displays the last 100 commands.
    *   **-t**: Shows the execution duration of each command in milliseconds.
*   `!n`: Re-executes command number *n* from history.
*   `!prefix`: Re-executes the last command starting with *prefix*.
*   `!!`: Re-executes the immediate previous command.

### 3. I/O Redirection & Pipelines
Smash supports complex command chaining:
*   `>`: Overwrite standard output to a file (e.g., `echo hello > file.txt`).
*   `>>`: Append standard output to a file (e.g., `echo world >> file.txt`).
*   `<`: Redirect standard input from a file (e.g., `cat < file.txt`).
*   `|`: Pipe the output of one command to the input of another (e.g., `ls | grep txt | wc -l`).

### 4. Process Management
*   **Background Jobs**: Ending a command with `&` runs it in the background, allowing the user to immediately enter new commands without waiting.
*   **Path Execution**: Uses a custom `execvp` logic to find binaries. It searches in the following priority:
    1.  Absolute/Relative path (e.g., `./script.sh`).
    2.  Root directory (e.g., `/ls`).
    3.  Current directory.

### 5. Scripting Support
*   **Batch Execution**: `smash script.sh` executes commands from a file.
*   **Shebang**: Kernel support for `#!/smash` allows scripts to be executed directly (e.g., `./script.sh`).
*   **Comments**: Lines starting with `#` are ignored.

---

## Kernel Modifications
To support the advanced features of Smash, several modifications were made to the xv6 kernel:

### 1. `sys_getcwd` (System Call)
*   **File**: `kernel/sysfile.c`, `user/user.h`
*   **Purpose**: Added a system call to retrieve the current working directory string from the process's `cwd` inode by traversing up to the root. This is required for the dynamic shell prompt.

### 2. `O_APPEND` Support
*   **File**: `kernel/fcntl.h`, `kernel/sysfile.c`
*   **Purpose**: Added the `O_APPEND` flag (0x004). Modified `sys_open` to detect this flag and set the file offset (`f->off`) to the file size (`ip->size`) immediately after opening, enabling the `>>` operator.

### 3. Shebang (`#!`) Support
*   **File**: `kernel/exec.c`
*   **Purpose**: Modified the `kexec` function. When loading a file, if the ELF magic number is missing, it checks the first two bytes for `#!`. If found, it parses the interpreter path (e.g., `/smash`) and recursively calls `kexec` to run the interpreter with the script as an argument.

---

## Implementation Details

### The Parsing Logic
The shell uses a custom tokenizer to split input by whitespace. It then parses the tokens in passes:
1.  **Background Check**: Checks if the last token is `&`.
2.  **Pipeline Split**: Splits the command into segments based on `|`.
3.  **Execution Loop**: Iterates through segments, creating pipes `pipe()` and forking `fork()` for each command.
4.  **Redirection**: Inside the child process, before execution, the arguments are scanned for `<`, `>`, `>>`. `close(0)` or `close(1)` are used followed by `open()` to replace file descriptors.

### History Implementation
History is stored in a global array of structs to prevent stack overflow. Each entry stores the command string and its execution duration. The duration is calculated using the `uptime()` system call (ticks converted to ms) before and after the wait loop.

---

## Testing
To compile and run the shell:

```bash
make clean
make qemu
```

Once inside xv6, start the shell:

```bash
$ smash
```

### Test Cases

**1. Redirection & Append:**
```bash
echo hello > test.txt
echo world >> test.txt
cat test.txt
# Output should be hello\nworld
```

**2. Pipes:**
```bash
ls | grep test
```

**3. Background Jobs:**
```bash
sleep 100 &
# Shell should immediately return prompt
```

**4. Scripting:**
```bash
# Create a file named test.sh:
#!/smash
echo "Running script"
# This is a comment
ls

# Run it:
./test.sh
```
