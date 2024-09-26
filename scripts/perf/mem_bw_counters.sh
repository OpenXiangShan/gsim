# From event list for memory for AMD 19h model 61h (EPYC 9004), which partially applies to 7950X3D

events="instructions,cycles"

# events=${events},"r740FE5F"  # memory read bw, where bit 22 is not supported
events=${events}",r700FE5F"  # memory read bw
events=${events}",r700FF5F"  # memory write bw
# events=${events}",r700FE1F,r700FE5F,r700FE9F,r700FE9F,r10700FE1F,r10700FE5F,r10700FE9F,r10700FE9F,r20700FE1F,r20700FE5F,r20700FE9F,r20700FE9F"  # memory read bw
# events=${events}",r700FF1F,r700FF5F,r700FF9F,r700FF9F,r10700FF1F,r10700FF5F,r10700FF9F,r10700FF9F,r20700FF1F,r20700FF5F,r20700FF9F,r20700FF9F"  # memory write bw

# events=${events}",r00FF71,r00FF72"  # L1D + L2 prefetches missed in L3
# events=${events}",r000104"  # L3 misses; IDK whether it works, because addr disclosed in offical doc contains some addr not supported
events=${events}",r000864"  # L2 data misses
events=${events}",r000164"  # L2 instr misses
# L2 refill
events=${events}",r000165"  # L2 fill From local CCX L3
events=${events}",r000265"  # L2 fill From another CCX L3
events=${events}",r000465"  # L2 fill From local numa node's dram, aka. demand L3 miss? not sure
events=${events}",r000865"  # L2 fill From another CCX L3, target another numa node; ???? why not zero on 7950x3d?
# events=${events}",r001065"  # Reserved; not zero on zen4
# events=${events}",r002065"  # 
# events=${events}",r004065"  # 
# events=${events}",r008065"  # 

# L1 Data Cache
# events=${events}",r000143"  # 
# events=${events}",r000243"  # 
# events=${events}",r000443"  # 
# events=${events}",r000843"  # 
# events=${events}",r001043"  # 
# events=${events}",r002043"  # 
# events=${events}",r004043"  # 
# events=${events}",r008043"  # 
