# Makefile for the comparator/filterator tools

VERS=0.1

CODE    = shredtree.c shredcompare.c main.c md5.c md5.h filterator
DOCS    = README comparator.xml
EXTRAS  = shredtree.py shredcompare.py
TEST    = test1 test2 test3 test4
SOURCES = $(CODE) $(DOCS) $(EXTRAS) $(TEST)

CFLAGS=-O

all: comparator comparator.1

main.o: main.c shred.h
	$(CC) -c $(CFLAGS) main.c 
shredtree.o: shredtree.c shred.h
	$(CC) -c $(CFLAGS) shredtree.c 
shredcompare.o: shredcompare.c shred.h
	$(CC) -c $(CFLAGS) shredcompare.c 
comparator: main.o shredtree.o md5.o shredcompare.o
	$(CC) main.o shredtree.o md5.o shredcompare.o -o comparator

clean:
	rm -f comparator shredtree.o md5.o shredcompare.o *~

comparator.1: comparator.xml
	xmlto man comparator.xml

test-a: comparator
	comparator test1 test2
test-b: comparator
	comparator test3 test4

install: comparator.1 uninstall
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/bin/
	install -m 755 -o 0 -g 0 comparator $(ROOT)/usr/bin/comparator
	install -m 755 -o 0 -g 0 filterator $(ROOT)/usr/bin/filterator
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/share/man/man1/
	install -m 755 -o 0 -g 0 comparator.1 $(ROOT)/usr/share/man/man1/comparator.1

uninstall:
	rm -f ${ROOT}/usr/bin/comparator 
	rm -f ${ROOT}/usr/bin/filterator 
	rm -f ${ROOT}/usr/share/man/man6/comparator.1

comparator-$(VERS).tar.gz: $(SOURCES) comparator.6
	@ls $(SOURCES) comparator.6 | sed s:^:comparator-$(VERS)/: >MANIFEST
	@(cd ..; ln -s comparator comparator-$(VERS))
	(cd ..; tar -czvf comparator/comparator-$(VERS).tar.gz `cat comparator/MANIFEST`)
	@(cd ..; rm comparator-$(VERS))

dist: comparator-$(VERS).tar.gz

RPMROOT=/usr/src/redhat
rpm: dist
	rpmbuild -ta comparator-$(VERS).tar.gz
	cp $(RPMROOT)/RPMS/*/comparator-$(VERS)*.rpm .
	cp $(RPMROOT)/SRPMS/comparator-$(VERS)*.src.rpm .
