// 文件：/include/linux/elevator.h

/*
 * each queue has an elevator_queue associated with it
 */
struct elevator_queue
{
	struct elevator_type *type;
	void *elevator_data;
	struct kobject kobj;
	struct mutex sysfs_lock;
	unsigned int registered:1;
	unsigned int uses_mq:1;
	DECLARE_HASHTABLE(hash, ELV_HASH_BITS);
};
