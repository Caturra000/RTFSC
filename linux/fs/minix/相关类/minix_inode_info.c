// 文件：/fs/minix/minix.h

/*
 * minix fs inode data in memory
 */
// 在module初始化时，minix_inode_info会分配一个minix_inode_cachep，
// bh->b_data可以得到struct minix_inode
// minix_inode_info和minix_inode的关联可以参考一下V1_minix_iget / minix_V1_raw_inode的流程
struct minix_inode_info {
	union {
		__u16 i1_data[16];
		__u32 i2_data[16];
	} u;
	// vfs用到的inode
	// 可以通过container_of重新得到minix_inode_info
	// 可以通过(struct minix_inode*)bh->b_data来构造vfs inode内部的各种参数
	struct inode vfs_inode;
};
