TARGET = rspiusb
OBJS = rspiusb.o
MDIR = drivers/misc

EXTRA_CFLAGS = -DEXPORT_SYMTAB
CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/kernel/$(MDIR)

# C_INCLUDE_PATH is an environment variable used by gcc
# to search for C header files. Here we prepend the path 
# to the kernel header files, it should be located somewhere 
# under /usr/src, and is usually something like
#   /usr/src/linux/include
# On Ubuntu 7.10 (Gutsy Gibbon) this path is:
#   /usr/src/linux-headers-2.6.22-14/include
C_INCLUDE_PATH = $(KDIR)/include $(C_INCLUDE_PATH)

obj-m      := $(TARGET).o

rspiusb:
	make -C $(KDIR) SUBDIRS=$(PWD)


default:
	make -C $(KDIR) SUBDIRS=$(PWD) modules

$(TARGET).o: $(OBJS)
	$(LD) $(LD_RFLAG) -r -o $@ $(OBJS)

ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	su -c "cp -v $(TARGET).o $(DEST) && /sbin/depmod -a"
else
install:
	su -c "cp -v $(TARGET).ko $(DEST) && /sbin/depmod -a"
endif

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

-include $(KDIR)/Rules.make
