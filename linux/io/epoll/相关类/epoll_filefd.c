struct epoll_filefd {
	struct file *file;
	int fd;
} __packed;