# Memory Allocation 101

I am learning how memory allocation works with the help of [this article](https://arjunsreedharan.org/post/148675821737/memory-allocators-101-write-a-simple-memory).

New:
- replaced sbrk with mmap
- implement coalesce
- boundary tag structure to store metadata at both the start and end of each block
- create array of free lists to manage free blocks of different sizes separately

Output of valgrind:
```
memalloc101 [ main][?][C v11.4.0-gcc]
❯ valgrind --leak-check=full ./test
==24741== Memcheck, a memory error detector
==24741== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==24741== Using Valgrind-3.18.1 and LibVEX; rerun with -h for copyright info
==24741== Command: ./test
==24741== 
Test String 1
Hello, World!
Hello, World!
==24741== 
==24741== HEAP SUMMARY:
==24741==     in use at exit: 0 bytes in 0 blocks
==24741==   total heap usage: 6 allocs, 6 frees, 1,818 bytes allocated
==24741== 
==24741== All heap blocks were freed -- no leaks are possible
==24741== 
==24741== For lists of detected and suppressed errors, rerun with: -s
==24741== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
