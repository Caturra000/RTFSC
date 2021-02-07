// include/linux/fs.h

struct fd {
	struct file *file;
	unsigned int flags;
};