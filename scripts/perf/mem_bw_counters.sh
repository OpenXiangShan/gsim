# full raw event list for memory for AMD 19h model 61h (EPYC 0094), which partially applies to 7950X3D
# events="r740FE1F,r740FE5F,r740FE9F,r740FE9F,r10740FE1F,r10740FE5F,r10740FE9F,r10740FE9F,r20740FE1F,r20740FE5F,r20740FE9F,r20740FE9F"  # memory read bw
# events=${events}",r740FF1F,r740FF5F,r740FF9F,r740FF9F,r10740FF1F,r10740FF5F,r10740FF9F,r10740FF9F,r20740FF1F,r20740FF5F,r20740FF9F,r20740FF9F"  # memory write bw

events="instructions,cycles"

# events=${events},"r740FE5F"  # memory read bw, where bit 22 is not supported
events=${events}",r700FE5F"  # memory read bw
events=${events}",r700FF5F"  # memory write bw

# We expect memory read = L3 (demand) misses + prefetches missed in L3, for double check
events=${events}",r000104"  # L3 misses
events=${events}",r00FF71,r00FF72"  # L1D + L2 prefetches missed in L3


