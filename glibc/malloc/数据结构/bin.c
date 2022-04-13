// 文件：malloc.c

/*
   Bins

    An array of bin headers for free chunks. Each bin is doubly
    linked.  The bins are approximately proportionally (log) spaced.
    There are a lot of these bins (128). This may look excessive, but
    works very well in practice.  Most bins hold sizes that are
    unusual as malloc request sizes, but are more usual for fragments
    and consolidated sets of chunks, which is what these bins hold, so
    they can be found quickly.  All procedures maintain the invariant
    that no consolidated chunk physically borders another one, so each
    chunk in a list is known to be preceeded and followed by either
    inuse chunks or the ends of memory.

    bin的大小常见于碎片以及合并过的chunk，而不直接适用于malloc？
    维护的不变式：bin之间不是物理相邻的，或者说相邻的chunk是处于inuse状态

    Chunks in bins are kept in size order, with ties going to the
    approximately least recently used chunk. Ordering isn't needed
    for the small bins, which all contain the same-sized chunks, but
    facilitates best-fit allocation for larger chunks. These lists
    are just sequential. Keeping them in order almost never requires
    enough traversal to warrant using fancier ordered data
    structures.

    bin一般是按大小排序的，small bin除外
    因为各small bin内每个chunk大小是一致的

    Chunks of the same size are linked with the most
    recently freed at the front, and allocations are taken from the
    back.  This results in LRU (FIFO) allocation order, which tends
    to give each chunk an equal opportunity to be consolidated with
    adjacent freed chunks, resulting in larger free chunks and less
    fragmentation.

    相同大小的bin使用FIFO的方式去维护，淘汰最不经常使用的bin
    Question. 为啥不按照LIFO来处理？这样应该局部性更好一点吧？
    Question. 为什么这里考虑公平性？这样做能达到更大的空闲chunk和更少的碎片？

    To simplify use in double-linked lists, each bin header acts
    as a malloc_chunk. This avoids special-casing for headers.
    But to conserve space and improve locality, we allocate
    only the fd/bk pointers of bins, and then use repositioning tricks
    to treat these as the fields of a malloc_chunk*.

    bin header视作chunk，但是只是用fd和bk（我猜是当前后链表的指针来用吧）
    这个需要结合bin_at宏来看，其实是bins数组中每2个元素才算一个bin（fd+bk），
    且各bin前2各用于伪装成chunk head（既prev_size+size），
    这样其它（真正的）chunk可以不知情地用自己的fd/bk指向伪装的chunk head（其实是对应bin的前一个bin）
    （大佬的代码就是不一样啊，头一次看是把数组不当数组用，再看一次发现还是把数组当别的数组用）
 */

typedef struct malloc_chunk *mbinptr;


/*
   Indexing

    Bins for sizes < 512 bytes contain chunks of all the same size, spaced
    8 bytes apart. Larger bins are approximately logarithmically spaced:

    64 bins of size       8
    32 bins of size      64
    16 bins of size     512
     8 bins of size    4096
     4 bins of size   32768
     2 bins of size  262144
     1 bin  of size what's left

   小于512字节的各个bin都有相同大小的chunk集合，每个量级差（公差）8大小
   既bins index和chunk size是线性关系
   而大于等于512的bin之间chunk大小公差分为6组，
   比如有一组共32各bin，前3各bin对应chunk size为512->576->640，公差64
   但后一组会剧增为公差512
   （bins数组中除bin0和bin1外，最前面62个bin（既index从2到63）均为公差8，起始bin大小为16）
   Note: 这里的注释说的应该是特指32位下的，
         回顾chunk.c中提到的multiple of 2 words，
         量级差应该至少是16而不能是8
   Note: 这里提到的bins index已经考虑了前面说的一个bin实际占用2个数组元素的背景，
         比如bin1就是定位于bin_at(m, 1)

    There is actually a little bit of slop in the numbers in bin_index
    for the sake of speed. This makes no difference elsewhere.

    The bins top out around 1MB because we expect to service large
    requests via mmap.

    Bin 0 does not exist.  Bin 1 is the unordered list; if that would be
    a valid chunk size the small bins are bumped up one.

    bin1是unsorted bin，基本是当垃圾桶用的，free的时候经常把要回收的内存往里扔
 */

#define NBINS             128

/* addressing -- note that bin_at(0) does not exist */
// 这里m的类型为mstate（一个typedef），展开为malloc_state*
// offsetof和kernel的同一个意思，不过gnu c有内置的__builtin_offsetof(TYPE, MEMBER)实现
#define bin_at(m, i) \
  (mbinptr) (((char *) &((m)->bins[((i) - 1) * 2]))			      \
             - offsetof (struct malloc_chunk, fd))
