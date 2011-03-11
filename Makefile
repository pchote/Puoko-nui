CC = gcc
CFLAGS = -g -c -Wall -pedantic -Dlinux --std=c99 -Ipvcam -D_POSIX_C_SOURCE=199309L $(shell pkg-config --cflags gtk+-2.0)
LFLAGS = -lcfitsio -lxpa -lpvcam -ldl -lpthread -lraw1394 -lftdi $(shell pkg-config --libs gtk+-2.0)

SRC = main.c camera.c view.c gps.c preferences.c
OBJ = $(SRC:.c=.o)

puokonui: $(OBJ)
	$(CC) $(LFLAGS) $(OBJ) libxpa.a -o $@

clean:
	-rm $(OBJ) puokonui

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
