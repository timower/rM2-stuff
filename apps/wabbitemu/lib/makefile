.SUFFIXES:
.SUFFIXES: .o .c

CFLAGS=-I. -I./core -I./interface -I./hardware -I./utilities\
		-DHIGH_SHADE_GIF -DVERBOSE -D_LINUX -std=gnu99
CC=gcc

.c.o:
	$(CC) -c $(CFLAGS) $*.c

all: wabbitemu

wabbitemu: ./core/*.c ./interface/*.c ./hardware/*.c ./utilities/gif.c \
 ./utilities/var.c utilities/gifhandle.c ./utilities/label.c ./utilities/savestate.cpp \
 ./utilities/sendfile.c
	$(CC) $(CFLAGS) -c ./core/*.c ./interface/*.c ./hardware/*.c ./utilities/gif.c \
 ./utilities/var.c utilities/gifhandle.c ./utilities/label.c ./utilities/savestate.cpp \
 ./utilities/sendfile.c

clean :
	rm *.o
