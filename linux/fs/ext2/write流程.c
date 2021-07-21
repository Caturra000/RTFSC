// 文件：/fs/ext2/file.c

static ssize_t ext2_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
#ifdef CONFIG_FS_DAX
	if (IS_DAX(iocb->ki_filp->f_mapping->host))
		return ext2_dax_write_iter(iocb, from);
#endif
	return generic_file_write_iter(iocb, from);
}
