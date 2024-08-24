CC := gcc
RM := find . -type f -name '*.o' -exec rm {} \; 
LIB := -Ilibs/http-parser -Ilibs/http-server
WARNFLAGS := -Werror -Wall -Wextra -pedantic -Wcast-align -Wcast-qual -Wdisabled-optimization -Wformat=2 -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wstrict-overflow=5 -Wundef -Wno-unused -Wno-variadic-macros -Wno-parentheses -fdiagnostics-show-option
CFLAGS := -fdiagnostics-color=always -std=c99 $(WARNFLAGS) $(LIB)
SRC := $(patsubst %.c,%.o,$(wildcard *.c *.h libs/*/*.c libs/*/*.h))

build: $(SRC)
	$(CC) $(CFLAGS) $(LIB) -o main.exe $^

debug: CFLAGS += -ggdb
debug: build

.PHONY: clean
clean:
	$(RM)