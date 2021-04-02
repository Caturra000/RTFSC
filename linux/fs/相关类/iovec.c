// 文件：/include/uapi/linux/uio.h
// 版本：4.18.20

// 用于传递数据，如readv
struct iovec
{
	void __user *iov_base;	/* BSD uses caddr_t (1003.1g requires void *) */
	__kernel_size_t iov_len; /* Must be size_t (1003.1g) */
};