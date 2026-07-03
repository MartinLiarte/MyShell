# MyShell - Unix Shell in C

**MyShell** is a feature-rich, custom Unix shell written in C as a systems programming project. It bridges the gap between a basic command interpreter and a production-ready shell by leveraging POSIX system calls, advanced tokenization, and the `GNU Readline` library.

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

* **Interactive Command Line:** Powered by `readline` with full support for navigation, **Tab autocompletion**, and persistent command history.
* **Pipelines (`|`):** Supports chaining multiple commands (up to 16 stages) to redirect the output of one command into the input of the next.
* **I/O Redirection:** Fully supports input redirection (`<`), output redirection (`>`), and append mode (`>>`).
* **Quote-Aware Tokenizer:** A robust state-machine parser that correctly handles arguments enclosed in single (`'`) and double (`"`) quotes, preserving spaces.
* **Intelligent Expansion:**
  * Expands environment variables (e.g., `$USER`, `$PATH`) inline, even inside double quotes.
  * Supports tilde (`~`) expansion to the user's `$HOME` directory.
* **Asynchronous Execution:** Supports running processes in the background using the trailing `&` operator, with safe zombie process cleanup (`SIGCHLD` handling).
* **Built-in Commands:**
  * `cd [dir]` — Changes directory (supports `cd -` to toggle to the previous directory).
  * `pwd` — Prints the current working directory.
  * `echo [-n]` — Prints text to stdout (with optional newline suppression).
  * `export VAR=val` — Sets environment variables.
  * `env` — Lists all current environment variables.
  * `history` — Shows the session's command history.
  * `clear` — Clears the terminal screen.
  * `exit [code]` — Terminate the shell cleanly.

---

## Cross-Platform Installation

The project includes a multi-platform `Makefile` that automatically detects your OS and correctly configures `readline` (including Homebrew paths on macOS).

### 1. Clone the repository
```bash
git clone https://github.com/MartinLiarte/MyShell.git
cd MyShell
```

### 2. Compile
Using `make` is highly recommended as it handles platform-specific linking:
```bash
make
```

*(Optional)* If you want to compile manually using GCC, you **must** link the readline library:
```bash
gcc -Wall -Wextra -Wpedantic -std=c11 MyShell.c -o myshell -lreadline
```

### 3. Run
```bash
./myshell
```
---
## Technical Details & Limitations

To maintain architectural simplicity, certain static thresholds are set:
* **Argument Limit:** Maximum of 64 arguments per command (`MAX_ARGS`). Excess arguments are safely truncated with a warning.
* **Pipeline Limit:** Maximum of 16 pipe stages per line (`MAX_PIPELINE`).
* **Subshell Isolation Notice:** Parent-altering builtins (like `cd` or `export`) executed inside a pipeline or with I/O redirections will run inside a forked subshell, meaning their environment changes will not persist in the main shell session (a warning will notify you).
---

## Cleaning up

```bash
make clean
```
Removes the compiled binary.

---
## Example Usage

```bash
MyShell(Desktop)> echo "Hello world" > file.txt
MyShell(Desktop)> cat file.txt | grep Hello
Hello world

MyShell(Desktop)> export PROJECT=MyShell
MyShell(Desktop)> echo "Working on $PROJECT" from ~
Working on MyShell from /Users/martin

MyShell(Desktop)> cd -
/Users/martin/PreviousFolder

MyShell(Desktop)> exit
Bye!
```

---

## Author

**Martin Liarte**
