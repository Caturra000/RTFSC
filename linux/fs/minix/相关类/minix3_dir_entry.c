// 文件：/include/uapi/linux/minix_fs.h

// 一般在实现中是使用minix3_dirent别名
typedef struct minix3_dir_entry minix3_dirent;

struct minix3_dir_entry {
	// 32位的inode number
	// 如在minix_readdir()中，inumber = de3->inode
	__u32 inode;
	// 虽然看似长度没有限制，
	// 但是在/fs/minix/dir.c#namecompare中是有maxlen限制的
	// maxlen来自于(struct minix_sb_info *) sbi->s_namelen
	// 允许长度为60（V3版本），具体见/fs/minix/inode.c#minix_fill_super
	char name[0];
};