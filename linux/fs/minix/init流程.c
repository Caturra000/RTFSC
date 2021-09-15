// 文件：/fs/minix/namei.c

// inode构造
static int minix_mknod(struct inode * dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	int error;
	struct inode *inode;

	if (!old_valid_dev(rdev))
		return -EINVAL;

	inode = minix_new_inode(dir, mode, &error);

	if (inode) {
		minix_set_inode(inode, rdev);
		mark_inode_dirty(inode);
		error = add_nondir(dentry, inode);
	}
	return error;
}


// 文件：/fs/minix/inode.c

// 可以找到各种operations
void minix_set_inode(struct inode *inode, dev_t rdev)
{
    // 常规文件
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &minix_file_inode_operations;
		inode->i_fop = &minix_file_operations;
		inode->i_mapping->a_ops = &minix_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &minix_dir_inode_operations;
		inode->i_fop = &minix_dir_operations;
		inode->i_mapping->a_ops = &minix_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &minix_symlink_inode_operations;
		inode_nohighmem(inode);
		inode->i_mapping->a_ops = &minix_aops;
	} else
		init_special_inode(inode, inode->i_mode, rdev);
}