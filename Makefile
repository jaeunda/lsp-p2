CC = gcc
CFLAGS = -Wall -g # all warning, debug

all: ssu_cleanupd arrange clean_obj

ssu_cleanupd: ssu_cleanupd.o util.o
	$(CC) $(CFLAGS) -o ssu_cleanupd ssu_cleanupd.o util.o

arrange: arrange.o util.o
	$(CC) $(CFLAGS) -o arrange arrange.o util.o

ssu_cleanupd.o: src/ssu_cleanupd.c src/header.h
	$(CC) $(CFLAGS) -c src/ssu_cleanupd.c

arrange.o: src/arrange.c src/header.h
	$(CC) $(CFLAGS) -c src/arrange.c

util.o: src/util.c src/header.h
	$(CC) $(CFLAGS) -c src/util.c

clean_obj:
	rm -f *.o

clean:
	rm -f ssu_cleanupd arrange
	
