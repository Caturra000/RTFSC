// 文件：/include/uapi/linux/uio.h
// 版本：4.18.20


// 详细说明见：https://lwn.net/Articles/625077/
// type:  the type of the iterator. It is a bitmask containing, among other things, 
//        either READ or WRITE depending on whether data is being read into the iterator or written from it.
// iov_offset: contains the offset to the first byte of interesting data in the first iovec pointed to by iov
// count: The total amount of data pointed to by the iovec array is stored in count

struct iov_iter {
	int type;
	size_t iov_offset;
	size_t count;
	union {
		const struct iovec *iov;
		const struct kvec *kvec;
		const struct bio_vec *bvec;
		struct pipe_inode_info *pipe;
	};
	union {
		unsigned long nr_segs;
		struct {
			int idx;
			int start_idx;
		};
	};
};