CC = gcc
CFLAGS = -g -c -Wall -pedantic -Dlinux --std=c99 -Ipvcam -D_POSIX_C_SOURCE=199309L
LFLAGS = -lncurses -lcfitsio -lxpa -lpvcam -ldl -lpthread -lraw1394 -lftdi

SRC = main.c camera.c gps.c preferences.c ui.c
OBJ = $(SRC:.c=.o)

puokonui: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LFLAGS)

clean:
	-rm $(OBJ) puokonui

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
