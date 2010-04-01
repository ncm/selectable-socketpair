# On Windows, add "-DWIN32"
CC=gcc
CFLAGS=-O3 -W -Wall -Wextra
LIB=socketpair

all: $(LIB).o

dummy:   # test win32 build on unix
	$(CC) -DWIN32 $(CFLAGS) -Idummy_headers -c -o $(LIB).dummy $(LIB).c
	rm $(LIB).dummy

clean:; rm $(LIB).o
