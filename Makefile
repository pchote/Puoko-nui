#CAMERA_TYPE = PVCAM
CAMERA_TYPE = NONE

CC = gcc
CFLAGS = -g -c -Wall -Wno-unknown-pragmas -pedantic -Dlinux --std=c99 -D_GNU_SOURCE
LFLAGS = -lpanel -lncurses -lcfitsio -lxpa -ldl -lpthread -lftdi
SRC = main.c camera.c gps.c preferences.c ui.c

ifeq ($(CAMERA_TYPE),PVCAM)
    CFLAGS += -Ipvcam -DUSE_PVCAM
    LFLAGS += -lpvcam -lraw1394
    SRC += camera_pvcam.c
endif

ifeq ($(MSYSTEM),MINGW32)
    CFLAGS += -Iftdi/include -Icfitsio/include -Incurses/include/ncurses
    LFLAGS += -Lftdi/lib -Lcfitsio/lib -Lncurses/lib
endif

OBJ = $(SRC:.c=.o)

puokonui: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LFLAGS)

clean:
	-rm $(OBJ) puokonui

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
