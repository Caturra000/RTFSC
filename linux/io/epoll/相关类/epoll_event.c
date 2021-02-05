struct epoll_event {
	__poll_t events;
	__u64 data;
} EPOLL_PACKED;