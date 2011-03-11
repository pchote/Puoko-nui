CC = gcc
CFLAGS = -g -c -Wall -pedantic -Dlinux --std=c99 -D_XOPEN_SOURCE -D_BSD_SOURCE
LFLAGS = -lcfitsio -arch x86_64

SRC = reduce.c framedata.c reduction.c
OBJ = $(SRC:.c=.o)

reduction: $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) -o $@

clean:
	-rm $(OBJ) reduction reduction.o framedata.o

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
