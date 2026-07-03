CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -std=c11 -O2 -g
TARGET  = myshell
SRC     = MyShell.c

# ── Detect readline flavour ───────────────────────────────────────────────
# On macOS, prefer Homebrew's GNU readline if available; fall back to libedit.
# On Linux, plain -lreadline links GNU readline.

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
    # Try Homebrew GNU readline first (arm64 and x86_64 paths)
    BREW_RL := $(shell brew --prefix readline 2>/dev/null)
    ifneq ($(BREW_RL),)
        CFLAGS  += -I$(BREW_RL)/include -DHAVE_RL_REPLACE_LINE
        LDFLAGS  = -L$(BREW_RL)/lib -lreadline
    else
        # Fall back to Apple's libedit (no rl_replace_line, no history_list)
        LDFLAGS  = -lreadline
    endif
else
    # Linux / other POSIX – assume GNU readline
    CFLAGS  += -DHAVE_RL_REPLACE_LINE
    LDFLAGS  = -lreadline
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)