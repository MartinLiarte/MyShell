# MyShell

**MyShell** is a custom shell written in C as a personal project.  
It features a green ASCII art banner at startup and supports executing external system commands.

---

## Features

- Startup banner in green ASCII art:
```
         __  ___      _____ __         ____
        /  |/  /_  __/ ___// /_  ___  / / /
       / /|_/ / / / /\__ \/ __ \/ _ \/ / / 
      / /  / / /_/ /___/ / / / /  __/ / /  
     /_/  /_/\__, //____/_/ /_/\___/_/_/   
            /____/                         

                  by Martin Liarte
```

- **Built-in commands:**
  - `cd [dir]` — change directory (supports `cd -` to go to previous directory)
  - `pwd` — print the current working directory
  - `clear` — clear the terminal
  - `exit` — exit the shell
- **Dynamic prompt:** Shows the current folder name, e.g., `MyShell(Desktop)>`
- **External commands:** Executes system commands using `fork()` and `execvp()`
- **Environment variable expansion:** Supports `$VAR` style variables (e.g., `$HOME`, `$PATH`)

---

## Limitations

- Environment variable expansion only works if the argument is **exactly `$VAR`**
- No support yet for:
  - Pipes (`|`)
  - Redirections (`>`, `<`)
  - Shell scripting
  - Tab autocompletion

---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/MartinLiarte/MyShell.git
cd MyShell
```

2. Compile:

 - Using MakeFile (recommended):
 
```bash
make
```
- Or manually with GCC:

```bash
gcc -Wall -Wextra -std=c99 MyShell.c -o myshell
```

3. Run:

```bash
./myshell
```
---

## Debugging

If you want a debug build with symbols for gdb:

```bash
make debug
./myshell
```
---

## Cleaning up

```bash
make clean
```
Removes the compiled binary.

---

## Example Usage

```bash
MyShell(Desktop)> pwd
/Users/username/Desktop

MyShell(Desktop)> cd ..
MyShell(Desktop)> pwd
/Users/username

MyShell(Desktop)> clear

MyShell(Desktop)> exit
```
---
## Notes

Temporary files like ~$filename.docx created by Word may appear in the directory when listing files. These are normal and can be safely ignored.

This shell is intended for learning and demonstration purposes. It’s a great way to practice system programming in C.

---

## Author

Martin Liarte
