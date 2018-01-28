This program simulates virtual memory page replacement algorithms and will output the number of page faults and swaps done to bring pages into memory given a tracefile with a particular algorithm.

There are four options:

__FIFO__: First in, first out page replacement

__OPT__: The absolutely optimal page replacement possible for the given tracefule

__CLOCK__: The clock page replacement algorithm

__NRU__: Not recently used page replacement algorithm

__RAND__: Select a random page to be replaced every time a page fault occurs

The results were surprisingly different with different values for __numframes__ - particularly because it was the first time I was exposed to [Bélády's anomaly](https://en.wikipedia.org/wiki/B%C3%A9l%C3%A1dy%27s_anomaly) outside of reading about it.

How to run:

```vmsim -n <numframes> -a <fifo|opt|clock|nru|rand> [-r <refresh>] <tracefile>```

This will require a tracefile. I believe GDB raw memory access tracefiles work.