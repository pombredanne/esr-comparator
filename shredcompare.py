#!/usr/bin/env python
#
# shredcompare -- find similarities in file trees.
#
# Implements the shred algorithm described at:
#
#	http://theinquirer.net/?article=10061
#
#   "Here is the procedure for finding the matching code.... "1. Each
#   file withing each source tree is "shredded" into 5 line pieces (1-5,
#   2-6, 3-7, etc.). MD5 sum is computed for each block of lines. The
#   output is 3 columns: MD5sum, source file, 1st line in the block.
#   
#   "At this stage, 4.4BSD had [a] ~40Mb file, linux ~160Mb. Potentially,
#   one could shred into smaller or larger pieces, however, with pieces
#   too small there'll be a lot of noise, with pieces too large some
#   matches won't be seen. 5 liners seem to be a good compromise.
#   
#   "2. Within each source tree the "shredded" file is sorted by MD5sum,
#   and duplicate entries within the same tree are removed completely
#   (these are either trivial 5-line sequences or licensing
#   disclaimers). Unix sort here takes a couple of minutes on a 600Mhz
#   P3.
#   
#   "3. A column indicating the origin of the file is inserted into the
#   file (0 - BSD, 1 - linux). Both Linux and BSD "shredded" files are
#   merged such that MD5sums stay sorted.
#   
#   "4. At this point a given MD5sum will occur either once or twice,
#   i.e., in both source trees. Here remove all thesingle lines, and have
#   the 5 liners left that are matching.
#   
#   "5. Count for each file in Linux tree the number of matches with the
#   BSD tree using the file generated at step 4. Sort this list, and the
#   largest counts will occur for the files with the largest number of
#   matching lines. The range can be extracted from the file from step 4,
#   since at step 1 we kept the address of the 1st line in the
#   block. That is how the info above was generated.
#
# In this implementation, lines are preprocessed to remove whitespace
# differences.
import sys, os, os.path, re, md5, getopt

shredsize = 5
ws = re.compile(r"\s+")

# These just make the code more readable.
name = 0
start = 1
end = 2
source = 0
target = 1

class Range:
    "Represent a range of lines."
    def __init__(self, file, start, end):
        self.file = file
        self.start = start
        self.end = end
    def __str__(self):
        # 0-origin line numbers internally, 1-origin externally.
        return "%s:%d-%d" % (self.file, self.start+1, self.end)
    __repr__ = __str__

def smash_whitespace(line):
    "Replace each string of whitespace in a line with a single space."
    return ws.sub(' ', line)

def shredfile(file, tree):
    "Build a dictionary of shred tuples corresponding to a specified file."
    shreds = {}
    fp = open(file)
    lines = map(smash_whitespace, fp.readlines())
    fp.close()
    for i in range(len(lines)-shredsize):
        m = md5.new()
        for j in range(shredsize):
            m.update(lines[i+j])
        # Merging shreds into a dict automatically suppresses duplicates
        shreds[m.digest()] = Range(file, i, i+shredsize)
    return shreds

def eligible(file):
    "Is a file eligible for comparison?"
    return filter(lambda x: file.endswith(x), ('.c', 'h', '.y', '.l', '.txt'))

def shredtree(tree):
    "Shred an entire file tree; return a dictionary of checksums-to-locations."
    shreds = {}
    # First, prepare the shred list.
    for root, dirs, files in os.walk(tree):
        for file in files:
            path = os.path.join(root, file)
            if eligible(path):
                shreds.update(shredfile(path, tree))
    return shreds

def shredcompare(tree1, tree2):
    "Compare two trees.  Returns a list of matching Range pairs"
    shreds1 = shredtree(tree1)
    shreds2 = shredtree(tree2)
    # Nuke everything but checksum matches between the trees
    for key in shreds1.keys():
        if not key in shreds2:
            del shreds1[key]
    for key in shreds2.keys():
        if not key in shreds1:
            del shreds2[key]
    # Merge the match lists
    matches = []
    for key in shreds1.keys():
        matches.append((shreds1[key], shreds2[key]))
    # Merge adjacent ranges if possible
    matches.sort()	# Sort in natural order
    merge_hit = True
    while merge_hit:
        merge_hit = False
        for i in range(1,len(matches)):
            j = len(matches) - i
            this = matches[j]
            last = matches[j-1]
            if last[source].file == this[source].file \
		   and last[target].file==this[target].file \
                   and last[source].start==this[source].start-1:
                merge_hit = True
                last[source].end += 1
                last[target].end +=1
                matches = matches[:j] + matches[j+1:]
    return matches

if __name__ == '__main__':
    try:
        (optlist, args) = getopt.getopt(sys.argv[1:], 's:')
    except getopt.GetoptError:
        sys.stderr.write("usage: shredcompare [-s shredsize] tree1 tree2\n")
        sys.exit(2)
    for (opt, val) in optlist:
        if opt == '-s':
            shredsize = int(val)
    matches = shredcompare(args[0], args[1])
    for (source, target) in matches:
        print source, "->", target

