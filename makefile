CC := gcc
LIB := -Ilibs/http-parser -Ilibs/http-server
CFLAGS := -Iutils -fdiagnostics-color=always -std=c99 -Wall -Wextra -Werror -Wmissing-prototypes -Wstrict-prototypes -ggdb $(LIB)
RM := rm -f
SRC := libs/http-parser/http_parser.o libs/http-server/http_server.o $(patsubst %.c,%.o,$(wildcard *.c))

build: $(SRC)
	$(CC) $(CFLAGS) $(LIB) -o main.exe $^

.PHONY: clean
clean:
	$(RM) *.o 