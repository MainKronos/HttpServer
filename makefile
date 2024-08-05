CC := gcc
CFLAGS := -Iutils -fdiagnostics-color=always -std=c11 -Wall -Wextra -Werror -Wmissing-prototypes -Wstrict-prototypes -ggdb -Ilibs/http-parser -Ilibs/http-server

build: main.c $(shell find . -name '*.c')
	$(CC) $(CFLAGS) -o main.exe $^

.PHONY: clean
clean:
	$(RM) *.o 