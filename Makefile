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
	time shredtree -c -w -d ~/src/unix src5r4 >src5r4.shif
	time shredtree -c -w -d ~/src/unix linux-2.6.0-test4 >linux-2.6.0-test4.shif
	time shredcompare.py src5r4.shif linux-2.6.0-test4.shif >TEST.LOG
