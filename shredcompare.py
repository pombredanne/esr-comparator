#!/usr/bin/env python
#
# shredcompare -- find similarities in file trees.
#
# 0.1 by Eric S. Raymond, 24 Aug 2003.
#
# Implements the shred algorithm described at:
#
#	http://theinquirer.net/?article=10061
#
# The -h option supports building a cache containing the shred list
# for a tree you specify.  This is strictly a speed hack.  If your
# tree is named `foo', the file will be named `foo.hash'.  On your
# next comparison run it will be picked up automatically. It is
# your responsibility to make sure each hash file is newer than
# the corresponding tree.
#
# This is alpha software. There is a bug in the implementation,
# somewhere in the logic for merging matching ranges before report
# generation, that sometimes garbages the last number in the
# target-tree range.  I haven't fixed it yet because I haven't found a
# way to produce a test load that reproduces it that is smaller that
# the entire Linux 2.6.o and Sysem V kernels.

import sys, os, os.path, re, md5, getopt, time, cPickle

shredsize = 5
verbose = False
mark_time = None
ws = re.compile(r"\s+")

class Range:
    "Represent a range of lines."
    def __init__(self, file, start, end):
        self.file = file
        self.start = start
        self.end = end
    def fetch(self):
        rfp = open(self.file)
        for i in range(self.start):
            rfp.readline()
        text = ""
        for i in range(self.start, self.end):
            text += rfp.readline()
        rfp.close()
        return text
    def __cmp__(self, other):
        return cmp((self.file, self.start, self.end), 
                   (other.file, other.start, other.end))
    def __str__(self):
        # 0-origin line numbers internally, 1-origin externally.
        return "%s:%d-%d" % (self.file, self.start+1, self.end)
    __repr__ = __str__

def smash_whitespace(line):
    "Replace each string of whitespace in a line with a single space."
    return ws.sub(' ', line)

def shredfile(file, tree):
    "Build a dictionary of shred tuples corresponding to a specified file."
    if verbose:
        print file
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
    if os.path.exists(tree + ".hash"):
        # There's a precomputed shred list.
        rfp = open(tree + ".hash")
        shreds = cPickle.load(rfp)
        rfp.close()
    else:
        # No precomputed hashes.  Prepare a news shred list.
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
        j = len(matches) - 1
        while j > 0:
            this = matches[j]
            last = matches[j-1]
            if last[0].file == this[0].file \
		   and last[1].file==this[1].file \
                   and last[0].start==this[0].start-1:
                merge_hit = True
                last[0].end = this[0].end
                last[1].end = this[1].end
                matches = matches[:j] + matches[j+1:]
            j -= 1
    return matches

def report_time(legend=None):
    "Report time since start_time was set."
    global mark_time
    endtime = time.time()
    if mark_time and verbose:
        elapsed = endtime - mark_time 
        hours = elapsed/3600; elapsed %= 3600
        minutes = elapsed/60; elapsed %= 60
        seconds = elapsed
        print "%% %s: %dh, %dm, %ds" % (legend, hours, minutes, seconds)
    mark_time = endtime

if __name__ == '__main__':
    try:
        (optlist, args) = getopt.getopt(sys.argv[1:], 'h:s:v')
    except getopt.GetoptError:
        sys.stderr.write("usage: shredcompare [-s shredsize] [-v] [-h] tree1 tree2\n")
        sys.exit(2)
    makehash = None
    for (opt, val) in optlist:
        if opt == '-s':
            shredsize = int(val)
        elif opt == '-v':
            verbose = True
        elif opt == '-h':
            makehash = val
    report_time()
    if makehash:
        shreds = shredtree(makehash)
        wfp = open(makehash + ".hash", "w")
        report_time("Shred list complete.")
        cPickle.dump(shreds, wfp)
        wfp.close()
        report_time("Pickling complete")
    else:
        matches = shredcompare(args[0], args[1])
        report_time("Match list complete")
        for (source, target) in matches:
            print source, "->", target
            if verbose:
                sys.stdout.write(source.fetch())

