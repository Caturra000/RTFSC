// 文件：/include/uapi/linux/minix_fs.h

// 注释部分参考了 Operating Systems: Design and Implementation P552的图
// 这个结构体on disk且in memory, always 1024 bytes
struct minix3_super_block {
	// inode的个数
	__u32 s_ninodes;
	// 未使用
	__u16 s_pad0;
	// inode bitmap blocks的个数
	__u16 s_imap_blocks;
	// zone bitmap blocks的个数
	__u16 s_zmap_blocks;
	__u16 s_firstdatazone;
	// log_2(block/zone)
	__u16 s_log_zone_size;
	// 未使用
	__u16 s_pad1;
	// 最大文件大小
	__u32 s_max_size;
	// zones个数
	__u32 s_zones;
	__u16 s_magic;
	__u16 s_pad2;
	// 按字节计算的block size
	__u16 s_blocksize;
	__u8  s_disk_version;
};