from time import time
from subprocess import call

for pt in range(0, 5):
  nbThreads = 2 ** pt
  for pp in range (0, 10):
    nbParts = 2 ** pp
    for r in range(-10, 11, 2):
      for i in range(-10, 11, 2):
        start = time()
        for j in range(0, 1):
          call(["./julia_bw", str(nbThreads), str(nbParts), str(r / 10), str(i / 10)])
        end = time()
        diff = end - start
        print('%d,%d,%f,%f,%f' % (nbThreads, nbParts, r / 10, i / 10, diff))
