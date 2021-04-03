// 文件：/include/linux/fs.h
// 版本：4.18.20

struct super_block {
	struct list_head	s_list;		/* Keep this first */ // 遍历所有sb对象
	dev_t			s_dev;		/* search index; _not_ kdev_t */ // 磁盘文件系统特有，存储sb的块设备编号
	unsigned char		s_blocksize_bits; // fs的块长度的位数
	unsigned long		s_blocksize; // fs的块长度
	loff_t			s_maxbytes;	/* Max file size */
	struct file_system_type	*s_type; // 属于（指回）哪个type
	const struct super_operations	*s_op;
	const struct dquot_operations	*dq_op; // 指向磁盘配额操作表的指针
	const struct quotactl_ops	*s_qcop; // 指向磁盘配额管理操作表的指针
	const struct export_operations *s_export_op; // 导出操作，应用于NFS
	unsigned long		s_flags; // 装载标志
	unsigned long		s_iflags;	/* internal SB_I_* flags */
	unsigned long		s_magic;
	struct dentry		*s_root; // fs根目录的dentry
	struct rw_semaphore	s_umount; // 卸载用信号量
	int			s_count; // 真正的引用计数，决定sb能否释放
	atomic_t		s_active; // 活动引用技数，表示被mount的次数
#ifdef CONFIG_SECURITY
	void                    *s_security;
#endif
	const struct xattr_handler **s_xattr; // 用于扩展
#if IS_ENABLED(CONFIG_FS_ENCRYPTION)
	const struct fscrypt_operations	*s_cop;
#endif
	struct hlist_bl_head	s_roots;	/* alternate root dentries for NFS */
	struct list_head	s_mounts;	/* list of mounts; _not_ for fs use */
	struct block_device	*s_bdev; // 磁盘文件系统特有，为指向块设备描述符的指针，其它fs为NULL
	struct backing_dev_info *s_bdi; // 指向后备信息描述符的指针
	struct mtd_info		*s_mtd;
	struct hlist_node	s_instances;
	unsigned int		s_quota_types;	/* Bitmask of supported quota types */
	struct quota_info	s_dquot;	/* Diskquota specific options */

	struct sb_writers	s_writers;

	char			s_id[32];	/* Informational name */ // 如果是磁盘文件系统，则为块设备名，否则为文件系统类型名
	uuid_t			s_uuid;		/* UUID */

	void 			*s_fs_info;	/* Filesystem private info */ // 私有实现
	unsigned int		s_max_links;
	fmode_t			s_mode; // 记录装载模式（只读，写/读）

	/* Granularity of c/m/atime in ns.
	   Cannot be worse than a second */
	u32		   s_time_gran;

	/*
	 * The next field is for VFS *only*. No filesystems have any business
	 * even looking at it. You had been warned.
	 */
	struct mutex s_vfs_rename_mutex;	/* Kludge */

	/*
	 * Filesystem subtype.  If non-empty the filesystem type field
	 * in /proc/mounts will be "type.subtype"
	 */
	char *s_subtype; // 应该应用于FUSE，内核不做区分，因此需要subtype提供给用户

	const struct dentry_operations *s_d_op; /* default d_op for dentries */

	/*
	 * Saved pool identifier for cleancache (-1 means none)
	 */
	int cleancache_poolid;

	struct shrinker s_shrink;	/* per-sb shrinker handle */

	/* Number of inodes with nlink == 0 but still referenced */
	atomic_long_t s_remove_count;

	/* Pending fsnotify inode refs */
	atomic_long_t s_fsnotify_inode_refs;

	/* Being remounted read-only */
	int s_readonly_remount;

	/* AIO completions deferred from interrupt context */
	struct workqueue_struct *s_dio_done_wq;
	struct hlist_head s_pins;

	/*
	 * Owning user namespace and default context in which to
	 * interpret filesystem uids, gids, quotas, device nodes,
	 * xattrs and security labels.
	 */
	struct user_namespace *s_user_ns;

	/*
	 * Keep the lru lists last in the structure so they always sit on their
	 * own individual cachelines.
	 */
	struct list_lru		s_dentry_lru ____cacheline_aligned_in_smp;
	struct list_lru		s_inode_lru ____cacheline_aligned_in_smp;
	struct rcu_head		rcu;
	struct work_struct	destroy_work;

	struct mutex		s_sync_lock;	/* sync serialisation lock */

	/*
	 * Indicates how deep in a filesystem stack this SB is
	 */
	int s_stack_depth;

	/* s_inode_list_lock protects s_inodes */
	spinlock_t		s_inode_list_lock ____cacheline_aligned_in_smp;
	struct list_head	s_inodes;	/* all inodes */ // 管inode的表头

	spinlock_t		s_inode_wblist_lock;
	struct list_head	s_inodes_wb;	/* writeback inodes */
} __randomize_layout;

// 超级块对象存在于两个链表
// s_list插入到管理所有的超级块对象的链表中，第一个元素为super_blocks
// s_instances插入到super_block所属的文件系统类型的超级块实例链表中