Critbit
=============

Critical-bit tree implementation, based on djb's critbit (http://cr.yp.to/critbit.html).

Benchmark
=============
Source tree includes various map data structure algorithms/implementations for benchmarking

Hash tables
 - redisdict: Dictionary used in Redis: https://github.com/antirez/redis/blob/unstable/src/dict.h
 - sparsehash: sparsehash from Google: https://code.google.com/p/sparsehash/
 - stluomap: libstdc++ std::unordered_map
 - uthash: Hash table for C structures: http://troydhanson.github.io/uthash/

Trees
 - stlmap: libstdc++ std::map
 - bsdtree: BSD red-black tree: https://github.com/freebsd/freebsd/blob/master/sys/sys/tree.h
 - ART: Adaptive Radix Trees: https://github.com/armon/libart
 - critbit: Critical-bit tree implementation
