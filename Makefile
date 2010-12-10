CC = gcc
CFLAGS = -g -Wall $(shell pkg-config --cflags --libs gtk+-2.0)
SRC = main.c

all: $(SRC)
	$(CC) $(CFLAGS) -o rangahau $(SRC)

