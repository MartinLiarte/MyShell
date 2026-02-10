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

- Built-in command:
- `exit` → exit the shell
- Support for **external system commands** (`ls`, `pwd`, `echo`, `cat`, etc.)
- Ignores empty input lines

---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/your_username/MyShell.git
cd MyShell
```

2. Compile with gcc:

```bash
gcc MyShell.c -o myshell
```

3. Run:

```bash
./myshell
```
