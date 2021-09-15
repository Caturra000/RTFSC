// 文件：/include/uapi/linux/minix_fs.h

/*
 * The new minix inode has all the time entries, as well as
 * long block numbers and a third indirect block (7+1+1+1
 * instead of 7+1+1). Also, some previously 8-bit values are
 * now 16-bit. The inode is now 64 bytes instead of 32.
 */
struct minix2_inode {
	// 文件类型（reg/dir等） / rwx
	__u16 i_mode;
	// Directory entries for this file
	__u16 i_nlinks;
	__u16 i_uid;
	__u16 i_gid;
	// Number of bytes in the file
	__u32 i_size;
	__u32 i_atime;
	__u32 i_mtime;
	__u32 i_ctime;
	// 0-6: Zone numbers for the first seven data zones in the file
	// 7: Indirect zone
	// 8: Double indirect zone
	// Unused: (Could be used for triple indirect zone)
	// (7-8: Used for files larger than 7 zones)
	__u32 i_zone[10];
};
