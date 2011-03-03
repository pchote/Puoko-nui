CC = gcc
CFLAGS = -g -c -Wall -pedantic -Dlinux --std=c99
LFLAGS = -lcfitsio

SRC = reduction.c
OBJ = $(SRC:.c=.o)

reduction: $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) -o $@

clean:
	-rm $(OBJ) reduction

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
