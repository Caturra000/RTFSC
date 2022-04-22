// 文件：/fs/block_dev.c
long nr_blockdev_pages(void)
{
	struct block_device *bdev;
	long ret = 0;
	spin_lock(&bdev_lock);
	// 可以看出，Buffers指的就是bdev的mapping统计
	// 其实它和page cache实现上是统一的
	// 因此算Cached时也要特意把Buffers部分减去（i.bufferram）
	// 可能是出于历史原因，兼容以前Buffers和Cached分离的接口
	list_for_each_entry(bdev, &all_bdevs, bd_list) {
		ret += bdev->bd_inode->i_mapping->nrpages;
	}
	spin_unlock(&bdev_lock);
	return ret;
}
