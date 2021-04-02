// 文件：/include/linux/fs.h
// 版本：4.18.20

// 表示IO control block，跟踪记录IO操作的完成状态
struct kiocb {
	struct file		*ki_filp;
	loff_t			ki_pos;
	void (*ki_complete)(struct kiocb *iocb, long ret, long ret2);
	void			*private;
	int			ki_flags;
	u16			ki_hint;
	u16			ki_ioprio; /* See linux/ioprio.h */
} __randomize_layout;