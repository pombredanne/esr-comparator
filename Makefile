# Makefile for shredcompare tools

SOURCES = shredtree.c shredcompare.c main.c md5.c md5.h

CFLAGS=-g

all: shredtree

main.o: main.c shred.h
	$(CC) -c $(CFLAGS) main.c 
shredtree.o: shredtree.c shred.h
	$(CC) -c $(CFLAGS) shredtree.c 
shredcompare.o: shredcompare.c shred.h
	$(CC) -c $(CFLAGS) shredcompare.c 
shredtree: main.o shredtree.o md5.o shredcompare.o
	$(CC) main.o shredtree.o md5.o shredcompare.o -o shredtree

clean:
	rm -f shredtree shredcompare shredtree.o md5.o shredcompare.o TEST1 TEST2

test-a: shredtree
	shredtree -f -c test1 >TEST1
	shredtree -f -c test2 >TEST2
	shredtree TEST1 TEST2
test-b: shredtree
	shredtree -f -c test3 >TEST1
	shredtree -f -c test4 >TEST2
	shredtree TEST1 TEST2

goodtest: shredtree
	shredtree -f -c -d ~/src/unix src5r4/src/ucbhead >src5r4.shif
	shredtree -f -c -d ~/src/unix linux-2.6.0-test4/include/linux >linux-2.6.0-test4.shif
	shredtree src5r4.shif linux-2.6.0-test4.shif

bigtest: shredtree
	shredtree -f -c -d ~/src/unix src5r4 >src5r4.shif
	shredtree -f -c -d ~/src/unix linux-2.6.0-test4 >linux-2.6.0-test4.shif
	shredtree src5r4.shif linux-2.6.0-test4.shif >TEST.LOG
