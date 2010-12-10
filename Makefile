all:
	gcc main.c -o rangahau `pkg-config --cflags --libs gtk+-2.0`
