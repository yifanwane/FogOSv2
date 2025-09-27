# FogOS

Fall 2025 Edition

![FogOS](docs/fogos.gif)

## 📋 Overview
This project extends the FogOS/xv6 `cat` utility with several Linux-style options to demonstrate user-space programming, system-call–based file I/O, and command-line parsing. Implemented flags include `-n` (number all lines), `-b` (number nonempty lines), `-E` (show `$` at end of each line), and `-s` (squeeze consecutive blank lines). The work preserves original `cat` behavior while adding composable formatting features.

## ✨ Implemented Features
- **`-n`** — Number **all** output lines  
- **`-b`** — Number **nonempty** output lines only  
- **`-E`** — Display `$` at the **end of each line**  
- **`-s`** — Squeeze multiple **consecutive blank lines** into a single blank line  

*Options can be combined.*  
Example:
```bash
cat -nEs file.txt

## 🛠 Build Instructions
From the **project root directory**, run:
```bash
make clean
make qemu

## ▶️ Run Instructions
Inside the FogOS shell:
```bash
cat [options] [file...]

If no file is provided, cat reads from standard input.

Multiple options can be combined and multiple files can be listed.

## 💡 Usage Examples
```bash
$ cat -n test.txt
  1  hello
  2  world

$ cat -bE test.txt
  1  hello$
     $
  2  world$

$ echo -e "foo\nbar" | cat -nEs
  1  foo$
  2  bar$

## ✅ Testing

### Manual Tests
The following commands were verified inside QEMU:
```bash
cat -n test.txt      # number all lines
cat -b test.txt      # number nonempty lines only
cat -E test.txt      # show $ at end of each line
cat -s test.txt      # squeeze consecutive blank lines
cat -nEs test.txt    # combine options
echo "abc" | cat -n  # read from stdin
cat -n file1 file2   # multiple files

Edge Cases

Empty files

Files with only blank lines

Consecutive blank lines

Large files to confirm no performance regressions

## 📂 Source Files
- `user/cat.c` — Enhanced implementation of the `cat` command  
- `Makefile` — Updated `UPROGS` list to include the rebuilt `cat`

---

## 🔖 Notes
- Code follows xv6/FogOS formatting standards (2-space indentation, minimal library usage).  
- Performance is equivalent to the original `cat` (no regressions).  
- This README provides all required documentation for building, running, and testing the software.
