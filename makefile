
# Tests we can't ship, because they depend on having real source trees handy.

include Makefile

# Test that making several hash files at one go doesn't mess up. 
UNIX = /home/esr/src/unix
SRC1  = net2
SRC2  = src5r4
regress2:
	comparator -C -d $(UNIX) $(SRC1) | dumpscf >test1-old.dump
	comparator -C -d $(UNIX) $(SRC2) | dumpscf >test2-old.dump
	comparator -C -c -d $(UNIX) $(SRC1) $(SRC2)
	dumpscf <$(SRC1).scf >test1.dump
	dumpscf <$(SRC2).scf >test2.dump
	diff -u test1-old.dump test1.dump
	diff -u test2-old.dump test2.dump

blackspot.ovl:
	time comparator -v -C -d $(UNIX) src5r4 linux-2.4.19 >blackspot.ovl
