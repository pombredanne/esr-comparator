# Makefile for the comparator/filterator tools
#
# By building with -DLARGEFILES you can go to all 32-byte offsets.
# This increases working-set size by 20% but handles > 65336 lines per file.

VERS=$(shell sed <comparator.spec -n -e '/Version: \(.*\)/s//\1/p')

CODE    = shredtree.c shred.h report.c hash.c linebyline.c main.c \
		hash.h hashtab.h filterator comparator.py 
SCRIPTS = hashgen.py setup.py
DOCS    = README comparator.xml scf-standard.xml COPYING
EXTRAS  = shredtree.py shredcompare.py
TEST    = test
SOURCES = $(CODE) $(SCRIPTS) $(DOCS) $(EXTRAS) $(TEST) comparator.spec Makefile
CFLAGS  = -O3
LDFLAGS = 

all: comparator comparator.1

main.o: main.c shred.h hash.h
	$(CC) -DVERSION=\"$(VERS)\" -c $(CFLAGS) main.c 
linebyline.o: linebyline.c
	$(CC) -c $(CFLAGS) linebyline.c
hash.o: hash.c hash.h hashtab.h
	$(CC) -c $(CFLAGS) hash.c 
shredtree.o: shredtree.c shred.h hash.h
	$(CC) -c $(CFLAGS) shredtree.c 
report.o: report.c shred.h hash.h
	$(CC) -c $(CFLAGS) report.c 
comparator: main.o hash.o linebyline.o shredtree.o report.o
	$(CC) $(CFLAGS) main.o hash.o linebyline.o shredtree.o report.o $(LDFLAGS) -o comparator

hashtab.h: hashgen.py
	python hashgen.py >hashtab.h

linebyline: linebyline.c
	$(CC) -DTEST $(CFLAGS) -o linebyline linebyline.c

clean:
	rm -f comparator linebyline *.o *~ comparator.1
	rm -f *.dump *.scf

comparator.1: comparator.xml
	xmlto man comparator.xml

comparator.html: comparator.xml
	xmlto html-nochunks comparator.xml

scf-standard.html: scf-standard.xml
	xmlto html-nochunks scf-standard.xml

OPTS="-N line-oriented, remove-braces, remove-whitespace"
makeregress:
	@for n in 1 2 3; do \
	    comparator $(OPTS) -d test test$${n}-a test$${n}-b | grep -v 'Merge-Program' >test/out$${n}.good;\
	done

regress:
	@for n in 1 2 3; do \
	    comparator $(OPTS) -d test test$${n}-a test$${n}-b | grep -v 'Merge-Program' >test/out$${n}.log;\
	    if diff -c test/out$${n}.good test/out$${n}.log; \
	    then \
		echo "Test $${n} from trees passed."; \
	    else \
		echo "Test $${n} from trees failed."; \
	    fi; \
	    comparator $(OPTS) -d test -c test$${n}-a >test$${n}-a.scf; \
	    comparator $(OPTS) -d test -c test$${n}-b >test$${n}-b.scf; \
	    comparator $(OPTS) test$${n}-a.scf test$${n}-b.scf >test/out$${n}.log;\
	    rm test$${n}-a.scf test$${n}-b.scf; \
	    if diff -u test/out$${n}.good test/out$${n}.log; \
	    then \
		echo "Test $${n} from SCFs passed."; \
	    else \
		echo "Test $${n} from SCFs failed."; \
	    fi; \
	done

install: comparator.1 uninstall
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/bin/
	install -m 755 -o 0 -g 0 comparator $(ROOT)/usr/bin/comparator
	install -m 755 -o 0 -g 0 filterator $(ROOT)/usr/bin/filterator
	install -m 755 -o 0 -g 0 -d $(ROOT)/usr/share/man/man1/
	install -m 755 -o 0 -g 0 comparator.1 $(ROOT)/usr/share/man/man1/comparator.1
	python setup.py install

uninstall:
	rm -f ${ROOT}/usr/bin/comparator 
	rm -f ${ROOT}/usr/bin/filterator 
	rm -f ${ROOT}/usr/share/man/man1/comparator.1
	#python setup.py uninstall

comparator-$(VERS).tar.gz: $(SOURCES) comparator.1
	find $(SOURCES) comparator.1 -type f | sed "s:^:comparator-$(VERS)/:" >MANIFEST
	(cd ..; ln -s comparator comparator-$(VERS))
	(cd ..; tar -czvf comparator/comparator-$(VERS).tar.gz `cat comparator/MANIFEST`)
	(cd ..; rm comparator-$(VERS))

dist: comparator-$(VERS).tar.gz

release: comparator-$(VERS).tar.gz comparator.html
	shipper -f; rm -f CHANGES ANNOUNCE* *.1 *.html *.rpm *.lsm
