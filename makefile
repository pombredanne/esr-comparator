
# Tests we can't ship, because they depend on having real source trees handy.

include Makefile

# These test trees should be nontrivial but not huge.
UNIX = /home/esr/src/unix
SRC1  = sys3
SRC2  = 32v

# Test that making several hash files at one go doesn't mess up. 
regress2:
	comparator -C -d $(UNIX) $(SRC1) | dumpscf >test1-old.dump
	comparator -C -d $(UNIX) $(SRC2) | dumpscf >test2-old.dump
	comparator -C -c -d $(UNIX) $(SRC1) $(SRC2)
	dumpscf <$(SRC1).scf >test1.dump
	dumpscf <$(SRC2).scf >test2.dump
	diff -u test1-old.dump test1.dump
	diff -u test2-old.dump test2.dump

# Test that making a report from SCF files works the same as from the trees
regress3:
	comparator -C -d $(UNIX) $(SRC1) $(SRC2) >test1.dump
	comparator -C -c -d $(UNIX) $(SRC1) $(SRC2)
	comparator $(SRC1).scf $(SRC2).scf >test2.dump
	diff -u test1.dump test2.dump

# Test SVr4 against Linux
sco.ovl:
	time comparator -v -C -d $(UNIX) src5r4 linux-2.4.19 >sco.ovl
