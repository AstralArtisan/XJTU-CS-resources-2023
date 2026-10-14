### Address space

**Definition**: Set of accessible addresses and the state associated with them

$$2^{32}$$ bytes: 32-bit address

**Memory multiplexing**:

- protection: prevent access to private memory of other processes
- translation: virtual to physical
- controlled overlap

***What does OS do to interpose on memory accesses?*** 

- Hardware: **MMU, do the translation** in common cases
- Software: Page fault, uncommon cases trap to OS

**Base and Bound**: two registers, storing the base address and bound address of an area. Compare the program address. Can use translation to add protection mechanism.

B&B issues: Fragmentation over time; Missing support for sparse address space



**Segmentation**: split the address space into multiple segments, each of which has a base and limit (the capacity of the segment), staying in a given region of contiguous memory.

Virtual segment address is split into seg ID and offset. According to the ID, processor gets the base and limit. Compare limit with offset. Base added to offset to generate physical address.

Translation happens when accessing to memory, on every instruction fetch, load or store. Segmentation can sparse the address space, for stack and heap to grow dynamically.(variable-sized) When context switch, segment table is stored in CPU, processes memory is **swapped** onto disk.

**Swapping** costs a lot. Needs paging to keep active portions of a process in memory.

**Fragmentation**:

- external: free gaps between allocated chunks
- internal: don't need all memory within allocated chunks



**Paging**: physical memory in fixed size chunks

use page table

When context switch, page table base register and limit need to change

10-10-12 structure page table

PTE:  pointer to next-level page table of to actual page

