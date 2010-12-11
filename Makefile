CC = gcc
CFLAGS = -g -c -ansi -Wall -O3 -pedantic -Wno-deprecated $(shell pkg-config --cflags gtk+-2.0) -Inative/include -Dlinux --std=c99 -D_POSIX_C_SOURCE=199309L
LFLAGS = -lcfitsio -lpvcam -ldl -lpthread -lraw1394 $(shell pkg-config --libs gtk+-2.0)

SRC = main.c camera.c acquisitionthread.c
OBJ = $(SRC:.c=.o)

rangahau: $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) -o $@

clean:
	-rm $(OBJ) rangahau

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
