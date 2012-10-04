CAMERA_TYPE := NONE
#CAMERA_TYPE := PVCAM
#CAMERA_TYPE := PICAM
GUI_TYPE := FLTK


GIT_SHA1 = $(shell sh -c 'git describe --dirty --always')
CC       = gcc
CXX      = g++
CFLAGS   = -g -c -Wall -Wno-unknown-pragmas -Iftd2xx --std=c99 -D_GNU_SOURCE -DGIT_SHA1=\"$(GIT_SHA1)\"
CXXFLAGS = -g -Wall -Wno-unknown-pragmas -pedantic
LFLAGS   = -lcfitsio -lpthread -lftd2xx -lm
OBJS     = main.o camera.o timer.o preferences.o scripting.o platform.o version.o

ifeq ($(CAMERA_TYPE),PVCAM)
	CFLAGS += -DUSE_PVCAM
	OBJS += camera_pvcam.o

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/PVCAM/SDK/ -lPvcam32
    else
        CFLAGS += -Dlinux -Ipvcam
        LFLAGS += -lpvcam -lraw1394 -ldl
    endif
endif

ifeq ($(CAMERA_TYPE),PICAM)
	CFLAGS += -DUSE_PICAM
	OBJS += camera_picam.o

    ifeq ($(MSYSTEM),MINGW32)
        CFLAGS += -Ic:/Program\ Files/Princeton\ Instruments/Picam/Includes
        LFLAGS += -Lc:/Program\ Files/Princeton\ Instruments/Picam/Libraries -lPicam
    else
        CFLAGS += -I/usr/local/picam/includes `apr-1-config --cflags --cppflags --includes`
        LFLAGS += -lpicam `apr-1-config --link-ld --libs`
    endif
endif

ifeq ($(GUI_TYPE),FLTK)
    CFLAGS += -DUSE_FLTK_GUI
    CXXFLAGS += $(shell fltk-config --cxxflags )
    LFLAGS += $(shell fltk-config --ldflags )
    OBJS += gui_fltk.o
else
    LFLAGS += -lpanel -lncurses
    OBJS += gui_ncurses.o
endif

# Statically link libgcc and libstdc++ to avoid needing extra dlls under windows
ifeq ($(MSYSTEM),MINGW32)
    CFLAGS += -DWIN32 -I/usr/local/include
    LFLAGS += -L/usr/local/lib -Lftd2xx/win32 -Lftd2xx/win64 -static-libgcc -static-libstdc++
endif

puokonui : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

clean:
	-rm $(OBJS) puokonui puokonui.exe

# Force version.o to be recompiled every time
version.o: .FORCE
.FORCE:
.PHONY: .FORCE

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $<

%.o : %.c
	$(CC) -c $(CFLAGS) $<
