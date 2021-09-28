// 文件：/block/noop-iosched.c

static struct elevator_type elevator_noop = {
	.ops.sq = {
		// 应用于elv_merge_requests
		.elevator_merge_req_fn		= noop_merged_requests,
		.elevator_dispatch_fn		= noop_dispatch,
		.elevator_add_req_fn		= noop_add_request,
		// 前一个请求
		.elevator_former_req_fn		= noop_former_request,
		// 后一个请求
		.elevator_latter_req_fn		= noop_latter_request,
		// 初始化
		.elevator_init_fn		= noop_init_queue,
		.elevator_exit_fn		= noop_exit_queue,
	},
	.elevator_name = "noop",
	.elevator_owner = THIS_MODULE,
};


// 入队操作
static void noop_add_request(struct request_queue *q, struct request *rq)
{
	// noop_data只有一个链表形式的queue成员
	struct noop_data *nd = q->elevator->elevator_data;

	// request只是简单插入request_queue中
	list_add_tail(&rq->queuelist, &nd->queue);
}

// 从noop链表移动到dispatch queue
static int noop_dispatch(struct request_queue *q, int force)
{
	struct noop_data *nd = q->elevator->elevator_data;
	struct request *rq;

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	if (rq) {
		// 刚才找到了，那从这里删掉
		list_del_init(&rq->queuelist);
		// rq放入q中
		elv_dispatch_sort(q, rq);
		return 1;
	}
	return 0;
}
