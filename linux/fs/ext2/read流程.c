
static ssize_t ext2_file_read_iter(struct kiocb *iocb, struct iov_iter *to)
{
#ifdef CONFIG_FS_DAX // 表示Direct Access
	if (IS_DAX(iocb->ki_filp->f_mapping->host))
		return ext2_dax_read_iter(iocb, to);
#endif
	// 该流程见vfs/read
	return generic_file_read_iter(iocb, to);
}



////////////////////////////////


static int ext2_readpage(struct file *file, struct page *page)
{
	// 见fs/block/read流程
	return mpage_readpage(page, ext2_get_block);
}
