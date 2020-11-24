/* Virtual memory object->where field. */
#define REDIS_VM_MEMORY 0       /* The object is on memory */
#define REDIS_VM_SWAPPED 1      /* The object is on disk */
#define REDIS_VM_SWAPPING 2     /* Redis is swapping this object on disk */
#define REDIS_VM_LOADING 3      /* Redis is loading this object from disk */


/* Virtual memory static configuration stuff.
 * Check vmFindContiguousPages() to know more about this magic numbers. */
#define REDIS_VM_MAX_NEAR_PAGES 65536
#define REDIS_VM_MAX_RANDOM_JUMP 4096
#define REDIS_VM_MAX_THREADS 32
#define REDIS_THREAD_STACK_SIZE (1024*1024*4)
/* The following is the *percentage* of completed I/O jobs to process when the
 * handelr is called. While Virtual Memory I/O operations are performed by
 * threads, this operations must be processed by the main thread when completed
 * in order to take effect. */
#define REDIS_MAX_COMPLETED_JOBS_PROCESSED 1



struct redisServer {

    // ...

    /* Virtual memory configuration */
    int vm_enabled;
    char *vm_swap_file;
    off_t vm_page_size;
    off_t vm_pages;
    unsigned long long vm_max_memory;
    /* Zip structure config */
    size_t hash_max_zipmap_entries;
    size_t hash_max_zipmap_value;
    size_t list_max_ziplist_entries;
    size_t list_max_ziplist_value;
    size_t set_max_intset_entries;
    /* Virtual memory state */
    FILE *vm_fp;
    int vm_fd;
    off_t vm_next_page; /* Next probably empty page */
    off_t vm_near_pages; /* Number of pages allocated sequentially */
    unsigned char *vm_bitmap; /* Bitmap of free/used pages */
    time_t unixtime;    /* Unix time sampled every second. */
    /* Virtual memory I/O threads stuff */
    /* An I/O thread process an element taken from the io_jobs queue and
     * put the result of the operation in the io_done list. While the
     * job is being processed, it's put on io_processing queue. */
    list *io_newjobs; /* List of VM I/O jobs yet to be processed */
    list *io_processing; /* List of VM I/O jobs being processed */
    list *io_processed; /* List of VM I/O jobs already processed */
    list *io_ready_clients; /* Clients ready to be unblocked. All keys loaded */
    pthread_mutex_t io_mutex; /* lock to access io_jobs/io_done/io_thread_job */
    pthread_mutex_t io_swapfile_mutex; /* So we can lseek + write */
    pthread_attr_t io_threads_attr; /* attributes for threads creation */
    int io_active_threads; /* Number of running I/O threads */
    int vm_max_threads; /* Max number of I/O threads running at the same time */
    /* Our main thread is blocked on the event loop, locking for sockets ready
     * to be read or written, so when a threaded I/O operation is ready to be
     * processed by the main thread, the I/O thread will use a unix pipe to
     * awake the main thread. The followings are the two pipe FDs. */
    int io_ready_pipe_read;
    int io_ready_pipe_write;
    /* Virtual memory stats */
    unsigned long long vm_stats_used_pages;
    unsigned long long vm_stats_swapped_objects;
    unsigned long long vm_stats_swapouts;
    unsigned long long vm_stats_swapins;

    // ...
};


/* Virtual Memory */
void vmInit(void);
void vmMarkPagesFree(off_t page, off_t count);
robj *vmLoadObject(robj *o);
robj *vmPreviewObject(robj *o);
int vmSwapOneObjectBlocking(void);
int vmSwapOneObjectThreaded(void);
int vmCanSwapOut(void);
void vmThreadedIOCompletedJob(aeEventLoop *el, int fd, void *privdata, int mask);
void vmCancelThreadedIOJob(robj *o);
void lockThreadedIO(void);
void unlockThreadedIO(void);
int vmSwapObjectThreaded(robj *key, robj *val, redisDb *db);
void freeIOJob(iojob *j);
void queueIOJob(iojob *j);
int vmWriteObjectOnSwap(robj *o, off_t page);
robj *vmReadObjectFromSwap(off_t page, int type);
void waitEmptyIOJobsQueue(void);
void vmReopenSwapFile(void);
int vmFreePage(off_t page);
void zunionInterBlockClientOnSwappedKeys(redisClient *c, struct redisCommand *cmd, int argc, robj **argv);
void execBlockClientOnSwappedKeys(redisClient *c, struct redisCommand *cmd, int argc, robj **argv);
int blockClientOnSwappedKeys(redisClient *c);
int dontWaitForSwappedKey(redisClient *c, robj *key);
void handleClientsBlockedOnSwappedKey(redisDb *db, robj *key);
vmpointer *vmSwapObjectBlocking(robj *val);