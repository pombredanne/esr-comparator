# Makefile for the comparator/filterator tools

VERS=1.5

CODE    = shredtree.c shred.h report.c main.c md5.c md5.h filterator
DOCS    = README NEWS comparator.xml scf-standard.xml COPYING
EXTRAS  = shredtree.py shredcompare.py
TEST    = test
SOURCES = $(CODE) $(DOCS) $(EXTRAS) $(TEST) comparator.spec Makefile
CFLAGS  = -O
LDFLAGS = 

all: comparator comparator.1

main.o: main.c shred.h
	$(CC) -DVERSION=\"$(VERS)\" -c $(CFLAGS) main.c 
shredtree.o: shredtree.c shred.h
	$(CC) -c $(CFLAGS) shredtree.c 
report.o: report.c shred.h
	$(CC) -c $(CFLAGS) report.c 
comparator: main.o shredtree.o md5.o report.o
	$(CC) $(CFLAGS) main.o shredtree.o md5.o report.o $(LDFLAGS) -o comparator

clean:
	rm -f comparator shredtree.o md5.o report.o main.o *~
	rm -f comparator.1 filterator.1 

comparator.1: comparator.xml
	xmlto man comparator.xml

comparator.html: comparator.xml
	xmlto html-nochunks comparator.xml

scf-standard.html: scf-standard.xml
	xmlto html-nochunks scf-standard.xml

test1: comparator
	@comparator -C -d test test1-a test1-b
test2: comparator
	@comparator -C -d test test2-a test2-b
test3: comparator
	@comparator -C -d test test3-a test3-b

install: comparator.1 uninstall
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/bin/
	install -m 755 -o 0 -g 0 comparator $(ROOT)/usr/bin/comparator
	install -m 755 -o 0 -g 0 filterator $(ROOT)/usr/bin/filterator
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/share/man/man1/
	install -m 755 -o 0 -g 0 comparator.1 $(ROOT)/usr/share/man/man1/comparator.1

uninstall:
	rm -f ${ROOT}/usr/bin/comparator 
	rm -f ${ROOT}/usr/bin/filterator 
	rm -f ${ROOT}/usr/share/man/man1/comparator.1

comparator-$(VERS).tar.gz: $(SOURCES) comparator.1
	find $(SOURCES) comparator.1 -type f | sed "s:^:comparator-$(VERS)/:" >MANIFEST
	(cd ..; ln -s comparator comparator-$(VERS))
	(cd ..; tar -czvf comparator/comparator-$(VERS).tar.gz `cat comparator/MANIFEST`)
	(cd ..; rm comparator-$(VERS))

dist: comparator-$(VERS).tar.gz

RPMROOT=/usr/src/redhat
rpm: dist
	rpmbuild --define 'version $(VERS)' -ta comparator-$(VERS).tar.gz
	cp $(RPMROOT)/RPMS/*/comparator-$(VERS)*.rpm .
	cp $(RPMROOT)/SRPMS/comparator-$(VERS)*.src.rpm .
