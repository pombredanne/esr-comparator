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

test-a:
	shredtree -c test1 >TEST1
	shredtree -c test2 >TEST2
	shredcompare TEST1 TEST2
test-b:
	shredtree -c test3 >TEST1
	shredtree -c test4 >TEST2
	shredcompare TEST1 TEST2

goodtest:
	shredtree -c -d ~/src/unix src5r4/src/ucbhead >src5r4.shif
	shredtree -c -d ~/src/unix linux-2.6.0-test4/include/linux >linux-2.6.0-test4.shif
	shredcompare src5r4.shif linux-2.6.0-test4.shif >TEST.LOG

bigtest:
	shredtree -c -d ~/src/unix src5r4 >src5r4.shif
	shredtree -c -d ~/src/unix linux-2.6.0-test4 >linux-2.6.0-test4.shif
	shredcompare src5r4.shif linux-2.6.0-test4.shif >TEST.LOG
