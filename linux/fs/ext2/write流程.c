// f_op
// 文件：/fs/ext2/file.c

static ssize_t ext2_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
#ifdef CONFIG_FS_DAX
	if (IS_DAX(iocb->ki_filp->f_mapping->host))
		return ext2_dax_write_iter(iocb, from);
#endif
	return generic_file_write_iter(iocb, from);
}






/////////////////////////////////////////////////////////







// a_op
// 文件：/fs/ext2/inode.c

static int
ext2_write_begin(struct file *file, struct address_space *mapping,
		loff_t pos, unsigned len, unsigned flags,
		struct page **pagep, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, flags, pagep,
				ext2_get_block);
	if (ret < 0)
		ext2_write_failed(mapping, pos + len);
	return ret;
}