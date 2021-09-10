// 文件：/include/linux/path.h

struct path {
	struct vfsmount *mnt;
	struct dentry *dentry;
} __randomize_layout;