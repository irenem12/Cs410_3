#!/usr/bin/env python3

import PyGnuplot as gp
import numpy as np
import sys
import os

directoryArg = str(sys.argv[1])
directory = os.listdir(directoryArg)

linkCount = 0
regularCount = 0
dirCount = 0
otherCount = 0

for file in directory:
    if os.path.islink(file):
        #file is symlink
        linkCount += 1
    elif os.path.isfile(file):
        #file is regular
        regularCount += 1
    elif os.path.isdir(file):
        #file is directory
        dirCount += 1
    else:
        #unhandled file type for now
        otherCount += 1

data = open("hist.dat", "w")
data.write(str.format("symlinks {}\n", linkCount))
data.write(str.format("files {}\n", regularCount))
data.write(str.format("directories {}\n", dirCount))
data.write(str.format("other {}\n", otherCount))
data.close()

gp.c("set term jpeg")
gp.c("set output 'hist.jpg'")
gp.c("set style data histograms")
gp.c("plot './hist.dat' using 2:xtic(1)")

with open("hist.jpg", 'rb') as jpg:
    print(jpg.read())
