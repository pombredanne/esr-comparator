#!/bin/env python
# Generate hash function mapping text bytes to an unsigned integral
# type, using MD5 to generate the magic values.  Based on an algorithm
# suggested by Ron Rivest.
#
# SPDX-License-Identifier: BSD-2-clause

import sys, hashlib

def tablegen(maxlen, width):
    print "static hashval_t magicbits[256][%d] = " % maxlen
    print "{"
    for i in range(256):
        sys.stdout.write('    /* char %02x */ {' % i)
        for j in range(maxlen):
            if j % 3 == 0:
                sys.stdout.write('\n\t')
            m = hashlib.md5()
            m.update(`i` + `j`)
            sys.stdout.write('0x' + m.hexdigest()[-(width*2):] + "ULL,")
        print "}, /* end char %02x */" % i
    print "};"


if __name__ == '__main__':
    maxlen = 512
    width = 8
    sys.stderr.write("Table will occupy %dK bytes\n"
                     % ((maxlen * width * 256) / 1024))
    tablegen(maxlen, width)

