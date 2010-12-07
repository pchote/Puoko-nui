#
# MakeFile for the native libraries used by the rangahau program.
# The native libraries are: 
#  1) A library to control a Roper camera, and
#  2) A library to control a GPS clock connected via USB. 
#
# Additional libraries required to build are:
#  libpvcam    (Roper/Princeton camera access library)
#  libdl       (used for dynamic linking)
#  libpthread  (portable POSIX threading library)
#  libraw1394  (raw access to 1394 Firewire, use the Ubuntu package 'libraw1394-dev')
#  libjava     (part of the Java Native Interface, obtained from Sun's Java Development Kit [JDK])
#  libusb      (used to talk to the GPS clock)
#  libftdi     (used to talk to the GPS clock)
#
# Some kernel modules are required to run the native libraries generated here:
#  * rspiusb is a kernel module that drives the Roper Scientific/Princeton Instruments camera,
#    as used by the libropercamera.so JNI library.
#  * usbcore is required by libropercamera.so
#
CC = g++ # the C++ compiler you want to use
LINKER = g++

#JAVA_HOME=/usr/lib/jvm/java-6-sun
JAVA_HOME=/usr/lib/jvm/java-6-openjdk

# This requires the JAVA_HOME environment variable to be set to the base
# directory of a Sun Java Development Kit (JDK), or OpenJDK (which is based on
# the Sun JDK). Examples JAVA_HOME locations are: 
#  /opt/java/jdk                (a custom JDK installation)
#  /usr/lib/jvm/java-6-openjdk  (OpenJDK 6 on Ubuntu)
#  /usr/lib/jvm/java-6-sun      (Sun JDK 6 on Ubuntu)                
INCLUDEPATH = -Iinclude -I$(JAVA_HOME)/include -I$(JAVA_HOME)/include/linux 
#CFLAGS = -O3 -Wall -fPIC
CFLAGS = -ggdb -Wall -fPIC

LIBPATH = -Llib -L$(JAVA_HOME)/jre/lib/i386
ROPER_LIBS = -lpvcam -ldl -lpthread -lraw1394 -ljava
GPSCLOCK_LIBS = -ljava -lftdi -lusb

LINKFLAGS = -O3


# C++ source and object files for the roper camera library.
ROPER_SOURCE = rangahau_RoperCamera.cpp
ROPER_OBJECTS = rangahau_RoperCamera.o

# C++ source and object files for the external timer device library.
GPSCLOCK_SOURCE = rangahau_GpsClock.cpp
GPSCLOCK_OBJECTS = rangahau_GpsClock.o

OBJECTS = $(ROPER_OBJECTS) $(GPSCLOCK_OBJECTS)

# Target programs.
TARGETS = libropercamera.so libgpsclock.so

all : $(TARGETS)


#---------------------------------------------------------------------------
# The ropercamera library providing the Java Native Interface backend to control a Roper Camera.
libropercamera.so : $(ROPER_OBJECTS)
	$(LINKER) -shared -Wl,-soname,$@ $(LINKFLAGS) $(ROPER_OBJECTS) $(LIBPATH) $(ROPER_LIBS) -o $@

#---------------------------------------------------------------------------
# The gpsclock library providing the Java Native Interface backend to control an 
# GPS controlled clock connected via USB.
libgpsclock.so : $(GPSCLOCK_OBJECTS)
	$(LINKER) -shared -Wl,-soname,$@ $(LINKFLAGS) $(GPSCLOCK_OBJECTS) $(LIBPATH) $(GPSCLOCK_LIBS) -o $@

#---------------------------------------------------------------------------
# Makes dependencies automatically and appends them to the end of this file.
depend:
	makedepend $(SOURCE)

# removes all object files
clean:
	-rm -f $(OBJECTS)
	@echo ''
	@echo finished removing object files
	@echo ''

	
#removes all object files and excutables
realclean:
	-rm -f $(OBJECTS)
	@echo ''
	@echo finished removing object files
	@echo ''
	-rm -f $(TARGETS)
	@echo ''
	@echo finished removing libraries
	@echo ''
 
# Add recognition of .cpp files (C++ source) that need to be converted 
# into object files (.o)
.SUFFIXES: .cpp
.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDEPATH) -c $< -o $@
 
# Give the user some help
help:
	@echo 'For installation do the following'
	@echo '     1) '\''make depend'\'' - to build source file dependencies.'
	@echo '     2) '\''make all'\'' - to build the library and executables.'
	@echo ''
	@echo 'The executables are:'
	@echo '    a) libropercamera.so - Java Native Interface wrapper to control a Roper camera'
	@echo '    b) libgpsclock.so - Java Native Interface wrapper to control an external GPS clock (via a USB port)'
	@echo ''
	@echo 'The command '\''make clean'\'' will remove intermediate'
	@echo 'object files, leaving the executables and source  behind.'
	@echo 'The command '\''make realclean'\''' will remove the object files'
	@echo 'executables leaving only the source behind.'
	@echo 'Mike Reid, 20 June 2009'

# The stuff below these comment lines is generated when 'make' depend is run
# DO NOT DELETE THIS LINE - make depend depends on it.
