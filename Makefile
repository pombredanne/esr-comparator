# Makefile for comparator tools

SOURCES = shredtree.c shredcompare.c main.c md5.c md5.h

CFLAGS=-g

all: comparator

main.o: main.c shred.h
	$(CC) -c $(CFLAGS) main.c 
shredtree.o: shredtree.c shred.h
	$(CC) -c $(CFLAGS) shredtree.c 
shredcompare.o: shredcompare.c shred.h
	$(CC) -c $(CFLAGS) shredcompare.c 
comparator: main.o shredtree.o md5.o shredcompare.o
	$(CC) main.o shredtree.o md5.o shredcompare.o -o comparator

clean:
	rm -f comparator shredtree.o md5.o shredcompare.o TEST1 TEST2

test-a: comparator
	comparator -e -c test1 test2
test-b: comparator
	comparator -e -c test3 test4
