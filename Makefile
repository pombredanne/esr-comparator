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
	comparator -f -c test1 >TEST1
	comparator -f -c test2 >TEST2
	comparator TEST1 TEST2
test-b: comparator
	comparator -f -c test3 >TEST1
	comparator -f -c test4 >TEST2
	comparator TEST1 TEST2

goodtest: comparator
	comparator -f -c -d ~/src/unix src5r4/src/ucbhead >src5r4.shif
	comparator -f -c -d ~/src/unix linux-2.6.0-test4/include/linux >linux-2.6.0-test4.shif
	comparator src5r4.shif linux-2.6.0-test4.shif

bigtest: comparator
	comparator -f -c -d ~/src/unix src5r4 >src5r4.shif
	comparator -f -c -d ~/src/unix linux-2.6.0-test4 >linux-2.6.0-test4.shif
	comparator src5r4.shif linux-2.6.0-test4.shif >TEST.LOG
