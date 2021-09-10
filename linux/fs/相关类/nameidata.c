// 文件：/fs/namei.c
// 版本：4.18.20

// 用于路径查找
struct nameidata {
	struct path	path;
	struct qstr	last; // 路径名中最后的分量
	struct path	root;
	struct inode	*inode; /* path.dentry.d_inode */
	unsigned int	flags;
	unsigned	seq, m_seq; // 顺序锁相关
	int		last_type; // 分量类型
	unsigned	depth; // 符号连接的嵌套级别
	int		total_link_count;
	struct saved {
		struct path link;
		struct delayed_call done;
		const char *name;
		unsigned seq;
	} *stack, internal[EMBEDDED_LEVELS]; // 递归相关
	struct filename	*name;
	struct nameidata *saved;
	struct inode	*link_inode;
	unsigned	root_seq;
	int		dfd;
} __randomize_layout;