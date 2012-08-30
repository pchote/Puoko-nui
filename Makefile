CAMERA_TYPE := NONE
#CAMERA_TYPE := PVCAM
#CAMERA_TYPE := PICAM
#XPA_TYPE := API
XPA_TYPE := EXTERNAL
#XPA_TYPE := NONE


CC = gcc
CFLAGS = -g -c -Wall -Wno-unknown-pragmas -pedantic -Dlinux --std=c99 -D_GNU_SOURCE
LFLAGS = -lpanel -lncurses -lcfitsio -lpthread -lftdi -lm
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

ifeq ($(CAMERA_TYPE),PICAM)
	CFLAGS += -DUSE_PICAM
	SRC += camera_picam.c

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/Picam/Includes
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/Picam/Libraries -lPicam
    else
        CFLAGS += -I/usr/local/picam/includes `apr-1-config --cflags --cppflags --includes`
        LFLAGS += -lpicam `apr-1-config --link-ld --libs`
    endif
endif

ifeq ($(XPA_TYPE),API)
    CFLAGS += -DUSE_XPA_API
    LFLAGS += -lxpa
endif

ifeq ($(XPA_TYPE),EXTERNAL)
    CFLAGS += -DUSE_XPA_EXTERNAL
endif

ifeq ($(MSYSTEM),MINGW32)
    CFLAGS += -DWIN32 -I/usr/local/include
    LFLAGS += -L/usr/local/lib
endif

OBJ = $(SRC:.c=.o)

puokonui: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LFLAGS)

clean:
	-rm $(OBJ) puokonui

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
