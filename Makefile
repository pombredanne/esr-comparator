# Makefile for shredcompare tools

SOURCES = shredtree.c md5.c md5.h

CFLAGS=-g

all: shredtree shredcompare

shredtree: shredtree.o md5.o
	$(CC) shredtree.o md5.o -o shredtree
shredtree.o: shredtree.c shred.h
	$(CC) -c $(CFLAGS) shredtree.c 

shredcompare.o: shredcompare.c shred.h
	$(CC) -c $(CFLAGS) shredcompare.c 
shredcompare: shredcompare.o
	$(CC) $(LFLAGS) shredcompare.o -o shredcompare

clean:
	rm -f shredtree shredcompare shredtree.o md5.o shredcompare.o

test:
	shredtree -c test1 >TEST1
	shredtree -c test2 >TEST2
	shredcompare TEST1 TEST2

testgood:
	shredtree -c test3 >TEST1
	shredtree -c test4 >TEST2
	shredcompare TEST1 TEST2
