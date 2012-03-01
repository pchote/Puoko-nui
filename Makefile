#CAMERA_TYPE = PVCAM
CAMERA_TYPE = NONE
USE_XPA = YES

CC = gcc
CFLAGS = -g -c -Wall -Wno-unknown-pragmas -pedantic -Dlinux --std=c99 -D_GNU_SOURCE
LFLAGS = -lpanel -lncurses -lcfitsio -lpthread -lftdi
SRC = main.c camera.c gps.c preferences.c ui.c platform.c

ifeq ($(CAMERA_TYPE),PVCAM)
	CFLAGS += -DUSE_PVCAM
	SRC += camera_pvcam.c

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/ -lPvcam32
    else
        CFLAGS += -Ipvcam
        LFLAGS += -lpvcam -lraw1394 -ldl
    endif
endif

ifeq ($(USE_XPA),YES)
    CFLAGS += -DUSE_XPA
    LFLAGS += -lxpa
endif

ifeq ($(MSYSTEM),MINGW32)
    CFLAGS += -DWIN32 -Iftdi/include -Icfitsio/include -Incurses/include/ncurses
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
